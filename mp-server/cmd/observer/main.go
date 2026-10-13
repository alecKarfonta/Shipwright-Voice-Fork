// [TIME] | [INFO] | observer | Live room verification: joins a room, prints the roster
package main

import (
	"bufio"
	"encoding/json"
	"fmt"
	"net"
	"os"
	"time"
)

func main() {
	room := "testroom"
	if len(os.Args) > 1 {
		room = os.Args[1]
	}
	tcp, err := net.Dial("tcp", "127.0.0.1:43383")
	if err != nil {
		fmt.Println("dial error:", err)
		os.Exit(1)
	}
	defer tcp.Close()

	send := func(obj map[string]interface{}) {
		b, _ := json.Marshal(obj)
		tcp.Write(append(b, 0))
	}
	send(map[string]interface{}{
		"type":        "HANDSHAKE",
		"roomId":      room,
		"clientState": map[string]interface{}{"name": "Observer", "teamId": "observers", "online": true},
	})

	rd := bufio.NewReader(tcp)
	deadline := time.Now().Add(5 * time.Second)
	for time.Now().Before(deadline) {
		tcp.SetReadDeadline(time.Now().Add(time.Until(deadline)))
		line, err := rd.ReadBytes(0)
		if err != nil {
			break
		}
		var p map[string]interface{}
		if json.Unmarshal(line[:len(line)-1], &p) != nil {
			continue
		}
		if p["type"] == "ALL_CLIENT_STATE" {
			fmt.Println("ROSTER:")
			for _, e := range p["state"].([]interface{}) {
				m := e.(map[string]interface{})
				fmt.Printf("  - %s (clientId=%v, online=%v, saveLoaded=%v, team=%v, self=%v)\n",
					m["name"], m["clientId"], m["online"], m["isSaveLoaded"], m["teamId"], m["self"])
			}
			os.Exit(0)
		}
	}
	fmt.Println("no roster received")
	os.Exit(1)
}
