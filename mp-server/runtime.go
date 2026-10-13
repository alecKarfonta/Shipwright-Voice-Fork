package main

import (
	"context"
	"errors"
	"flag"
	"fmt"
	"log"
	"net"
	"net/http"
	"os"
	"os/signal"
	"sync"
	"syscall"
	"time"
)

// Covers TCP and hijacked WebSocket connections, which http.Server.Shutdown
// does not close. Admission and drain share a lock to avoid orphaned sessions.
var lifecycle = struct {
	sync.Mutex
	draining bool
	clients  map[net.Conn]bool
}{clients: map[net.Conn]bool{}}

func trackConnection(c net.Conn) bool {
	lifecycle.Lock()
	defer lifecycle.Unlock()
	if lifecycle.draining {
		return false
	}
	lifecycle.clients[c] = true
	return true
}
func untrackConnection(c net.Conn) {
	lifecycle.Lock()
	delete(lifecycle.clients, c)
	lifecycle.Unlock()
}
func isDraining() bool {
	lifecycle.Lock()
	defer lifecycle.Unlock()
	return lifecycle.draining
}

func serveTCP(ln net.Listener) error {
	for {
		tcp, err := ln.Accept()
		if err != nil {
			return err
		}
		if tc, ok := tcp.(*net.TCPConn); ok {
			tc.SetKeepAlive(true)
			tc.SetKeepAlivePeriod(30 * time.Second)
		}
		if !admitConnection() {
			tcp.Close()
			continue
		}
		go func() { defer releaseConnection(); handleTCP(tcp) }()
	}
}

func main() {
	flag.Parse()
	log.SetPrefix("[hyrule-relay] ")
	if *portFlag == 0 && *httpFlag == "" {
		log.Fatal("enable TCP or HTTP")
	}
	ctx, stop := signal.NotifyContext(context.Background(), os.Interrupt, syscall.SIGTERM)
	defer stop()
	errorsCh := make(chan error, 2)
	var tcpListener net.Listener
	var httpServer *http.Server
	if *portFlag != 0 {
		var err error
		tcpListener, err = net.Listen("tcp", net.JoinHostPort(*bindFlag, fmt.Sprint(*portFlag)))
		if err != nil {
			log.Fatal(err)
		}
		log.Printf("TCP listening on %s", tcpListener.Addr())
		go func() { errorsCh <- serveTCP(tcpListener) }()
	}
	if *httpFlag != "" {
		httpServer = &http.Server{Addr: *httpFlag, Handler: relayHTTPHandler(), ReadHeaderTimeout: 5 * time.Second, IdleTimeout: 60 * time.Second, MaxHeaderBytes: 8192}
		go func() { errorsCh <- httpServer.ListenAndServe() }()
	}
	select {
	case <-ctx.Done():
	case err := <-errorsCh:
		if !errors.Is(err, net.ErrClosed) && !errors.Is(err, http.ErrServerClosed) {
			log.Printf("listener stopped: %v", err)
		}
	}
	lifecycle.Lock()
	lifecycle.draining = true
	clients := make([]net.Conn, 0, len(lifecycle.clients))
	for c := range lifecycle.clients {
		clients = append(clients, c)
	}
	lifecycle.Unlock()
	if tcpListener != nil {
		tcpListener.Close()
	}
	log.Printf("Draining %d connections", len(clients))
	for _, c := range clients {
		go c.Close()
	}
	shutdown, cancel := context.WithTimeout(context.Background(), 8*time.Second)
	defer cancel()
	if httpServer != nil {
		httpServer.Shutdown(shutdown)
	}
	ticker := time.NewTicker(20 * time.Millisecond)
	defer ticker.Stop()
	for {
		lifecycle.Lock()
		remaining := len(lifecycle.clients)
		lifecycle.Unlock()
		if remaining == 0 {
			return
		}
		select {
		case <-shutdown.Done():
			return
		case <-ticker.C:
		}
	}
}
