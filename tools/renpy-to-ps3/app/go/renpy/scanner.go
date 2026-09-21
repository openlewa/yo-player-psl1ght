package renpy

import (
	"strconv"
	"strings"
)

// ScanClasses collects Python classes referenced in a pickle byte stream
// without reconstructing objects.
func ScanClasses(b []byte) map[string]struct{} {
	return (&classScanner{
		classes: map[string]struct{}{},
		memo:    map[int64]string{},
	}).run(b)
}

type classScanner struct {
	classes                    map[string]struct{}
	memo                       map[int64]string
	memoCounter                int64
	last1, last2, lastProduced string
	hasLast1, hasLast2         bool
	producedIsNull             bool
}

func (s *classScanner) produce(str string) {
	s.lastProduced = str
	s.producedIsNull = false
	if str != "" || true {
		s.last1 = s.last2
		s.hasLast1 = s.hasLast2
		s.last2 = str
		s.hasLast2 = true
	}
}

func (s *classScanner) produceNull() {
	s.lastProduced = ""
	s.producedIsNull = true
}

func (s *classScanner) memoGet(idx int64) (string, bool) {
	v, ok := s.memo[idx]
	return v, ok
}

func (s *classScanner) run(b []byte) map[string]struct{} {
	i, n := 0, len(b)
	for i < n {
		op := b[i]
		i++
		switch op {
		case 0x80:
			i++
		case 0x95:
			i += 8
		case 0x8c:
			ln := int(b[i])
			i++
			s.produce(utf8Slice(b[i : i+ln]))
			i += ln
		case 'X':
			ln := i32(b, &i)
			s.produce(utf8Slice(b[i : i+ln]))
			i += ln
		case 0x8d:
			ln := int(i64(b, &i))
			s.produce(utf8Slice(b[i : i+ln]))
			i += ln
		case 'U':
			ln := int(b[i])
			i++
			s.produce(latin1(b[i : i+ln]))
			i += ln
		case 'T':
			ln := i32(b, &i)
			s.produce(latin1(b[i : i+ln]))
			i += ln
		case 'S':
			e := newlineEnd(b, i)
			s.produce(latin1(b[i:e]))
			i = e + 1
		case 'V':
			e := newlineEnd(b, i)
			s.produce(utf8Slice(b[i:e]))
			i = e + 1
		case 'C':
			ln := int(b[i])
			i++
			i += ln
			s.produceNull()
		case 'B':
			ln := i32(b, &i)
			i += ln
			s.produceNull()
		case 0x8e, 0x96:
			ln := int(i64(b, &i))
			i += ln
			s.produceNull()
		case 'I', 'L', 'F':
			i = newlineEnd(b, i) + 1
			s.produceNull()
		case 'J':
			i += 4
			s.produceNull()
		case 'K':
			i++
			s.produceNull()
		case 'M':
			i += 2
			s.produceNull()
		case 'G':
			i += 8
			s.produceNull()
		case 0x8a:
			ln := int(b[i])
			i++
			i += ln
			s.produceNull()
		case 0x8b:
			ln := i32(b, &i)
			i += ln
			s.produceNull()
		case 0x94:
			if s.producedIsNull {
				s.memo[s.memoCounter] = ""
			} else {
				s.memo[s.memoCounter] = s.lastProduced
			}
			s.memoCounter++
		case 'q':
			idx := int64(b[i])
			i++
			if s.producedIsNull {
				s.memo[idx] = ""
			} else {
				s.memo[idx] = s.lastProduced
			}
		case 'r':
			idx := int64(i32(b, &i))
			if s.producedIsNull {
				s.memo[idx] = ""
			} else {
				s.memo[idx] = s.lastProduced
			}
		case 'p':
			e := newlineEnd(b, i)
			idx := plong(b, i, e)
			if s.producedIsNull {
				s.memo[idx] = ""
			} else {
				s.memo[idx] = s.lastProduced
			}
			i = e + 1
		case 'h':
			idx := int64(b[i])
			i++
			if v, ok := s.memoGet(idx); ok {
				s.produce(v)
			} else {
				s.produceNull()
			}
		case 'j':
			idx := int64(i32(b, &i))
			if v, ok := s.memoGet(idx); ok {
				s.produce(v)
			} else {
				s.produceNull()
			}
		case 'g':
			e := newlineEnd(b, i)
			if v, ok := s.memoGet(plong(b, i, e)); ok {
				s.produce(v)
			} else {
				s.produceNull()
			}
			i = e + 1
		case 'c':
			e1 := newlineEnd(b, i)
			mod := latin1(b[i:e1])
			i = e1 + 1
			e2 := newlineEnd(b, i)
			nm := latin1(b[i:e2])
			i = e2 + 1
			s.classes[mod+"."+nm] = struct{}{}
			s.produceNull()
		case 0x93:
			if s.hasLast1 && s.hasLast2 {
				s.classes[s.last1+"."+s.last2] = struct{}{}
			}
			s.produceNull()
		case 0x82:
			i++
		case 0x83:
			i += 2
		case 0x84:
			i += 4
		default:
		}
	}
	return s.classes
}

func plong(b []byte, s, e int) int64 {
	v, err := strconv.ParseInt(latin1(b[s:e]), 10, 64)
	if err != nil {
		return -1
	}
	return v
}

func classSetKeys(m map[string]struct{}) []string {
	out := make([]string, 0, len(m))
	for k := range m {
		out = append(out, k)
	}
	return out
}

var _ = strings.Join
