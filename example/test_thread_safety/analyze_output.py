#!/usr/bin/env python3
"""
Analyze thread safety test output for corruption patterns.
Usage: pio run -t monitor | tee output.log
       python3 analyze_output.py output.log
"""

import re
import sys
from collections import defaultdict, Counter

class LogAnalyzer:
    def __init__(self):
        self.backends = {}
        self.current_backend = None
        self.corruption_patterns = {
            'interleaved': [],
            'broken_timestamp': [],
            'partial_message': [],
            'malformed_line': [],
            'out_of_sequence': defaultdict(list)
        }
        
    def parse_log_line(self, line):
        """Parse a log line and return its components."""
        # Standard log format: [timestamp][task][level] tag: message
        pattern = r'^\[(\d+)\]\[([^\]]+)\]\[([A-Z])\]\s*([^:]+):\s*(.*)$'
        match = re.match(pattern, line)
        if match:
            return {
                'timestamp': int(match.group(1)),
                'task': match.group(2),
                'level': match.group(3),
                'tag': match.group(4),
                'message': match.group(5),
                'raw': line
            }
        return None
        
    def check_message_sequence(self, parsed):
        """Check if worker messages are in sequence."""
        if not parsed or 'Worker' not in parsed['tag']:
            return
            
        # Extract message number from MSG_XXX pattern
        msg_match = re.search(r'MSG_(\d+)_START', parsed['message'])
        if msg_match:
            worker_id = parsed['tag']
            msg_num = int(msg_match.group(1))
            
            if worker_id not in self.backends[self.current_backend]['sequences']:
                self.backends[self.current_backend]['sequences'][worker_id] = []
            
            seq = self.backends[self.current_backend]['sequences'][worker_id]
            if seq and msg_num != seq[-1] + 1:
                self.corruption_patterns['out_of_sequence'][self.current_backend].append({
                    'worker': worker_id,
                    'expected': seq[-1] + 1,
                    'got': msg_num,
                    'line': parsed['raw']
                })
            seq.append(msg_num)
    
    def analyze_line(self, line):
        """Analyze a single line for corruption patterns."""
        line = line.strip()
        if not line:
            return
            
        # Detect backend switch
        if "Testing" in line and "Backend" in line:
            match = re.search(r'Testing (\w+Backend)', line)
            if match:
                self.current_backend = match.group(1)
                self.backends[self.current_backend] = {
                    'lines': [],
                    'corruptions': [],
                    'sequences': {},
                    'message_count': 0
                }
                return
                
        if not self.current_backend:
            return
            
        # Store line for backend
        self.backends[self.current_backend]['lines'].append(line)
        
        # Try to parse as standard log
        parsed = self.parse_log_line(line)
        
        if parsed:
            # Count messages
            if 'MSG_' in parsed['message'] or 'FLOOD_' in parsed['message']:
                self.backends[self.current_backend]['message_count'] += 1
                
            # Check sequence for workers
            self.check_message_sequence(parsed)
            
            # Check for incomplete messages
            if 'MSG_' in parsed['message']:
                if not parsed['message'].endswith(re.search(r'MSG_(\d+)', parsed['message']).group(1)):
                    self.corruption_patterns['partial_message'].append({
                        'backend': self.current_backend,
                        'line': line
                    })
        else:
            # Line doesn't match standard format - potential corruption
            if any(x in line for x in ['[', 'MSG_', 'FLOOD_', 'Task', 'Worker']):
                self.corruption_patterns['malformed_line'].append({
                    'backend': self.current_backend,
                    'line': line
                })
                
        # Check for obvious interleaving (multiple timestamps in one line)
        timestamp_count = len(re.findall(r'\[\d+\]', line))
        if timestamp_count > 1:
            self.corruption_patterns['interleaved'].append({
                'backend': self.current_backend,
                'line': line,
                'timestamps': timestamp_count
            })
            
        # Check for broken timestamps
        if line.startswith('[') and not re.match(r'^\[\d+\]', line):
            self.corruption_patterns['broken_timestamp'].append({
                'backend': self.current_backend,
                'line': line
            })
    
    def generate_report(self):
        """Generate analysis report."""
        print("\n" + "="*60)
        print("THREAD SAFETY TEST ANALYSIS REPORT")
        print("="*60)
        
        for backend in self.backends:
            print(f"\n### {backend}")
            print(f"Total lines: {len(self.backends[backend]['lines'])}")
            print(f"Message count: {self.backends[backend]['message_count']}")
            
            # Count corruptions for this backend
            corruptions = 0
            
            # Interleaved messages
            interleaved = [x for x in self.corruption_patterns['interleaved'] 
                          if x['backend'] == backend]
            if interleaved:
                print(f"\nInterleaved messages: {len(interleaved)}")
                for i, item in enumerate(interleaved[:3]):  # Show first 3
                    print(f"  Example {i+1}: {item['line'][:80]}...")
                if len(interleaved) > 3:
                    print(f"  ... and {len(interleaved)-3} more")
                corruptions += len(interleaved)
            
            # Broken timestamps
            broken_ts = [x for x in self.corruption_patterns['broken_timestamp'] 
                        if x['backend'] == backend]
            if broken_ts:
                print(f"\nBroken timestamps: {len(broken_ts)}")
                for i, item in enumerate(broken_ts[:3]):
                    print(f"  Example {i+1}: {item['line'][:80]}...")
                corruptions += len(broken_ts)
            
            # Partial messages
            partial = [x for x in self.corruption_patterns['partial_message'] 
                      if x['backend'] == backend]
            if partial:
                print(f"\nPartial messages: {len(partial)}")
                for i, item in enumerate(partial[:3]):
                    print(f"  Example {i+1}: {item['line'][:80]}...")
                corruptions += len(partial)
            
            # Malformed lines
            malformed = [x for x in self.corruption_patterns['malformed_line'] 
                        if x['backend'] == backend]
            if malformed:
                print(f"\nMalformed lines: {len(malformed)}")
                for i, item in enumerate(malformed[:3]):
                    print(f"  Example {i+1}: {item['line'][:80]}...")
                corruptions += len(malformed)
            
            # Out of sequence
            if backend in self.corruption_patterns['out_of_sequence']:
                oos = self.corruption_patterns['out_of_sequence'][backend]
                if oos:
                    print(f"\nOut of sequence messages: {len(oos)}")
                    for i, item in enumerate(oos[:3]):
                        print(f"  {item['worker']}: expected {item['expected']}, got {item['got']}")
                    corruptions += len(oos)
            
            # Summary
            if corruptions == 0:
                print(f"\n✅ RESULT: No corruption detected")
            else:
                print(f"\n❌ RESULT: {corruptions} corruption patterns detected")
            
            # Check message sequences
            print("\nWorker message sequences:")
            for worker in sorted(self.backends[backend]['sequences'].keys()):
                seq = self.backends[backend]['sequences'][worker]
                if seq:
                    print(f"  {worker}: {len(seq)} messages (first:{seq[0]}, last:{seq[-1]})")
                    if len(seq) != 50:
                        print(f"    WARNING: Expected 50 messages, got {len(seq)}")
        
        print("\n" + "="*60)
        print("SUMMARY")
        print("="*60)
        
        # Overall corruption summary
        total_corruptions = sum([
            len(self.corruption_patterns['interleaved']),
            len(self.corruption_patterns['broken_timestamp']),
            len(self.corruption_patterns['partial_message']),
            len(self.corruption_patterns['malformed_line']),
            sum(len(v) for v in self.corruption_patterns['out_of_sequence'].values())
        ])
        
        print(f"Total corruption patterns found: {total_corruptions}")
        
        # Backend comparison
        print("\nBackend Comparison:")
        for backend in self.backends:
            backend_corruptions = sum([
                len([x for x in self.corruption_patterns['interleaved'] if x['backend'] == backend]),
                len([x for x in self.corruption_patterns['broken_timestamp'] if x['backend'] == backend]),
                len([x for x in self.corruption_patterns['partial_message'] if x['backend'] == backend]),
                len([x for x in self.corruption_patterns['malformed_line'] if x['backend'] == backend]),
                len(self.corruption_patterns['out_of_sequence'].get(backend, []))
            ])
            
            status = "✅ CLEAN" if backend_corruptions == 0 else f"❌ {backend_corruptions} issues"
            print(f"  {backend:30} {status}")
        
        print("\n" + "="*60)

def main():
    if len(sys.argv) < 2:
        print("Usage: python3 analyze_output.py <log_file>")
        print("       pio run -t monitor | tee output.log")
        sys.exit(1)
    
    analyzer = LogAnalyzer()
    
    try:
        with open(sys.argv[1], 'r') as f:
            for line in f:
                analyzer.analyze_line(line)
    except FileNotFoundError:
        print(f"Error: File '{sys.argv[1]}' not found")
        sys.exit(1)
    
    analyzer.generate_report()

if __name__ == "__main__":
    main()