#!/usr/bin/env python2

# fwclient.py - Client to the fwctl daemon

# Copyright(c) 2016 Jeff Spaulding <sarnet@gmail.com>
# Permission to use, copy, modify, and distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.


import sys
import getopt
import socket


# -----------------------------------------------------------------------------
# Functions

# Print the help message
def print_help():
    print "fwclient.py <-s server_address> <-p port> <-c command> [-h]"


# Send a command to the server and return the response
# See README for valid commands and responses
def do_command(server, port, command):
    success = False
    s = []
    error = ''
    buf = ''
    sender = ''

    try:
        dest_gai = socket.getaddrinfo(server, port, socket.AF_UNSPEC,
                                      socket.SOCK_DGRAM)
    except socket.gaierror as e:
        print "Error resolving hostname:", e.strerror

    for i, dest_addr in enumerate(dest_gai):
        try:
            s = socket.socket(dest_addr[0], socket.SOCK_DGRAM)
            s.settimeout(1)

            s.sendto(command, dest_addr[4])
            buf, sender = s.recvfrom(3)

            success = True
            s.close()
            break

        except socket.error as e:
            error = e.strerror
            s.close()
            continue

    if success is False:
        buf = error[:]

    return buf


# -----------------------------------------------------------------------------
# Code begins here

# Parse options
def main():
    server = ''
    command = ''
    port = 2287

    try:
        options, remainder = getopt.getopt(sys.argv[1:], 's:p:c:h')
    except getopt.error:
        print_help()
        sys.exit()

    for opt, arg in options:
        if opt == '-s':
            server = arg
        elif opt == '-p':
            port = arg
        elif opt == '-c':
            command = arg
        elif opt == '-h':
            print "fwclient.py <-s server_address> <-p server_port>",
            print "<-c command> [-h]"
            sys.exit()

    if (server == '') or (command == ''):
        print_help()
        sys.exit()

    if command == 'FST':
        status = do_command(server, port, command)
        if status == 'SUP':
            print "Anchor rules are loaded."
        elif status == 'SDN':
            print "Anchor rules are clear."
        elif status == 'ERR':
            print "The server returned an error."
        else:
            print "An error occurred:", status
    elif (command == 'FDN') or (command == 'FUP'):
        status = do_command(server, port, command)
        if status == 'ACK':
            print "Command succeeded."
        else:
            print "Command failed."
    else:
        print "Unrecognized command."

if __name__ == "__main__":
    main()
