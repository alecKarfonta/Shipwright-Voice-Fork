package main

import (
	"bytes"
	"encoding/json"
	"io"
	"net"
	"sync"
	"sync/atomic"
	"time"
)

const maxPendingBytes = 2 * 1024 * 1024
const maxGlobalPendingBytes = 64 * 1024 * 1024

var pendingBytes atomic.Int64
var slowDisconnects atomic.Uint64

// A single writer preserves ordering. Producers never wait for socket I/O,
// including when a producer holds a room lock. Overflow disconnects the peer:
// silently dropping a reliable item or quest event would corrupt its state.
type outbox struct {
	tcp    net.Conn
	mu     sync.Mutex
	bytes  int64
	closed bool
	queue  chan []byte
	done   chan struct{}
}

func newOutbox(tcp net.Conn) *outbox {
	o := &outbox{tcp: tcp, queue: make(chan []byte, 128), done: make(chan struct{})}
	go o.run()
	return o
}

func (o *outbox) enqueue(frame []byte) {
	o.mu.Lock()
	defer o.mu.Unlock()
	if o.closed {
		return
	}
	n := int64(len(frame))
	if o.bytes+n > maxPendingBytes || len(o.queue) == cap(o.queue) {
		o.closeLocked(true)
		return
	}
	if pendingBytes.Add(n) > maxGlobalPendingBytes {
		pendingBytes.Add(-n)
		o.closeLocked(true)
		return
	}
	o.bytes += n
	o.queue <- frame
}

func (o *outbox) release(n int) {
	o.mu.Lock()
	o.bytes -= int64(n)
	pendingBytes.Add(-int64(n))
	o.mu.Unlock()
}

func (o *outbox) closeLocked(slow bool) {
	if o.closed {
		return
	}
	o.closed = true
	if slow {
		slowDisconnects.Add(1)
	}
	close(o.queue)
	// WebSocket Close can wait for the closing handshake. Do not hold up a room.
	go o.tcp.Close()
}

func (o *outbox) stop() {
	o.mu.Lock()
	o.closeLocked(false)
	o.mu.Unlock()
	<-o.done
}

func (o *outbox) run() {
	defer close(o.done)
	failed := false
	for frame := range o.queue {
		if !failed {
			o.tcp.SetWriteDeadline(time.Now().Add(5 * time.Second))
			if _, err := io.Copy(o.tcp, bytes.NewReader(frame)); err != nil {
				failed = true
				o.mu.Lock()
				o.closeLocked(false)
				o.mu.Unlock()
			}
		}
		o.release(len(frame))
	}
}

// Only used before admission, when this connection has no queued writes.
func (c *conn) reject(message string) {
	b, _ := json.Marshal(map[string]interface{}{"type": "SERVER_MESSAGE", "message": message})
	c.tcp.SetWriteDeadline(time.Now().Add(time.Second))
	io.Copy(c.tcp, bytes.NewReader(append(b, 0)))
	c.tcp.Close()
}
