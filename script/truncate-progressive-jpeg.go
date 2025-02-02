// Copyright 2023 The Wuffs Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

//go:build ignore
// +build ignore

package main

// truncate-progressive-jpeg.go saves truncated editions of a JPEG, cutting
// just after every SOS (Start Of Scan) marker, appending an EOI (End Of Image)
// marker after the cut.
//
// Usage: go run truncate-progressive-jpeg.go foo.jpeg

import (
	"fmt"
	"os"
	"strings"
)

func main() {
	if err := main1(); err != nil {
		os.Stderr.WriteString(err.Error() + "\n")
		os.Exit(1)
	}
}

func main1() error {
	if len(os.Args) < 2 {
		return fmt.Errorf("usage: progname filename.jpeg")
	}
	src, err := os.ReadFile(os.Args[1])
	if err != nil {
		return err
	}
	prefix, suffix := os.Args[1], ".jpeg"
	if strings.HasSuffix(prefix, ".jpg") {
		prefix = prefix[:len(prefix)-4]
		suffix = ".jpg"
	} else if strings.HasSuffix(prefix, ".jpeg") {
		prefix = prefix[:len(prefix)-5]
	}

	if len(src) < 2 {
		return fmt.Errorf("invalid JPEG")
	}
	scanNumber := 0
	for pos := 0; pos <= (len(src) - 2); {
		if src[pos] != 0xFF {
			return fmt.Errorf("invalid JPEG")
		}
		marker := src[pos+1]
		pos += 2
		switch {
		case (0xD0 <= marker) && (marker < 0xD9):
			// RSTn (Restart) and SOI (Start Of Image) markers have no payload.
			continue
		case marker == 0xD9:
			// EOI (End Of Image) marker.
			return nil
		}

		if pos >= (len(src) - 2) {
			return fmt.Errorf("invalid JPEG")
		}
		payloadLength := (int(src[pos]) << 8) | int(src[pos+1])
		pos += payloadLength
		if (payloadLength < 2) || (pos < 0) || (pos > len(src)) {
			return fmt.Errorf("invalid JPEG")
		}

		if marker != 0xDA { // SOS (Start Of Scan) marker.
			continue
		}
		for pos < len(src) {
			if src[pos] != 0xFF {
				pos += 1
			} else if pos >= (len(src) - 1) {
				return fmt.Errorf("invalid JPEG")
			} else if src[pos+1] != 0x00 {
				break
			} else {
				pos += 2
			}
		}
		outName := fmt.Sprintf("%s.scan%03d%s", prefix, scanNumber, suffix)
		scanNumber++
		data := ([]byte)(nil)
		data = append(data, src[:pos]...)
		data = append(data, "\xFF\xD9"...) // Append an EOI marker.
		if err := os.WriteFile(outName, data, 0666); err != nil {
			return err
		}
		fmt.Printf("Wrote %s (cut at 0x%08X = %6d bytes).\n", outName, pos, pos)
	}
	return fmt.Errorf("invalid JPEG")
}
