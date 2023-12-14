import subprocess
import re

def bench(file, sender_port, router_port, receiver_port, arq, sender_n, receiver_n, timeout):
    command = ['./build/bench_cli', '--file', file, '--sender_port', str(sender_port), '--router_port', str(router_port), '--receiver_port', str(receiver_port), '--arq', arq, '--sender_n='+str(sender_n), '--receiver_n='+str(receiver_n), '--timeout='+str(timeout)]
    try:
        output = subprocess.check_output(command, stderr=subprocess.STDOUT)
    except subprocess.CalledProcessError as e:
        print(f'An error occurred while running the C++ program: {e}')
        return None
    else:
        output = output.decode('utf-8')
        # print(output)
        regex = r'FTPReceiver: Throughput: (\d+\.?\d*) B/s'
        matches = re.findall(regex, output)
        return float(matches[0])

file = "testcases/3.jpg"
sender_port = 8100
router_port = 8099
receiver_port = 8089
Ns = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 32]
throughputs = []

for N in Ns:
    # run 3 times and get the average
    ret = 0
    for i in range(5):
        ret += bench(file, sender_port, router_port, receiver_port, 'sr', N, N, 100)
    ret /= 5
    throughputs.append(ret)
    print(f'N = {N} Throughput = {ret}')

# print in pairs
for i in range(len(Ns)):
    print(f'N = {Ns[i]} Throughput = {throughputs[i]}')

