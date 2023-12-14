# Reference: Router script from JuniMay 
# https://github.com/JuniMay/nkurdt/blob/main/router.py
# Check https://github.com/JuniMay/nkurdt
# Awesome work from my classmate!
import socket
import argparse
import random
import time


def router_main():
    argparser = argparse.ArgumentParser()
    argparser.add_argument("-p", "--port", type=int, default=3333)
    argparser.add_argument("-i", "--ip", type=str, default="127.0.0.1")
    argparser.add_argument("-rp", "--remote-port", type=int, default=4321)
    argparser.add_argument("-ri", "--remote-ip", type=str, default="127.0.0.1")
    argparser.add_argument("-l", "--loss", type=float, default=0.03)
    argparser.add_argument("-d", "--delay", type=float, default=5)
    argparser.add_argument('-ds', '--delay-server', action='store_true')

    args = argparser.parse_args()

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind((args.ip, args.port))
    print("router is running on {}:{}".format(args.ip, args.port))

    client_addr = None

    total_count = 0
    loss_count = 0

    # async socket, for windows, blocking socket cannot be interrupted by KeyboardInterrupt
    # sock.setblocking(False)

    try:
        while True:
            try:
                data, addr = sock.recvfrom(65535)
                if addr[0] != args.remote_ip or addr[1] != args.remote_port:
                    if client_addr is None:
                        client_addr = addr
                        print("client address is {}".format(client_addr))

                    loss_packet = random.uniform(0, 1) < args.loss
                    delay = abs(random.normalvariate(args.delay, 0.5))

                    total_count += 1

                    if loss_packet:
                        print("loss packet from client")
                        loss_count += 1
                        continue
                    else:
                        print("delay packet from client for {:.5} ms".format(delay))
                        time.sleep(delay / 1000)
                        sock.sendto(data, (args.remote_ip, args.remote_port))

                elif client_addr is not None:
                    if (args.delay_server):
                    # also simulate
                        loss_packet = random.uniform(0, 1) < args.loss
                        delay = abs(random.normalvariate(args.delay, 0.5))
                        total_count += 1

                        if loss_packet:
                            print("loss packet from server")
                            loss_count += 1
                            continue

                        else:
                            print("delay packet from server for {:.5} ms".format(delay))
                            time.sleep(delay / 1000)
                            sock.sendto(data, client_addr)
                    else:
                        sock.sendto(data, client_addr)

            except socket.error:
                pass
    except KeyboardInterrupt:
        print(
            "total packet: {}, loss packet: {}, loss rate: {}".format(
                total_count, loss_count, loss_count / total_count
            )
        )


if __name__ == "__main__":
    router_main()