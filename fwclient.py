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
    print "fwclient.py <-s server_address> <-p port> [-u | -d] [-h] [-q]"
    print "  -u Load anchor rules"
    print "  -d Clear anchor rules"
    print "  -q Supress output"
    print "Anchor status is returned when neither -u or -d is specified."
    sys.exit()


# Send a command to the server and return the response
# See README for valid commands and responses
# Returns EADDR if it cannot resolve the server address
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
        return 'EADDR'

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
# Interactive code below

# Parse options
def main():
    server = ''
    command = 'FST'
    port = 2287
    uflag = False
    dflag = False
    qflag = False
    success = True

    try:
        options, remainder = getopt.getopt(sys.argv[1:], 's:p:udh')
    except getopt.error:
        print_help()
        sys.exit()

    for opt, arg in options:
        if opt == '-s':
            server = arg
        elif opt == '-p':
            port = arg
        elif opt == '-d':
            command = "FDN"
            dflag = True
        elif opt == '-u':
            command = "FUP"
            uflag = True
        elif opt == '-q':
            qflag = True
        elif opt == '-h':
            print_help()

    if (uflag and dflag):
        print "Cannot specify -u and -d at the same time."
        print_help()

    if (server == ''):
        print "-s flag is required."
        print_help()

    status = do_command(server, port, command)

    if status == 'ACK':
        if not qflag:
            print "Command succeded."
    elif status == 'ERR':
        if not qflag:
            print "Server returned an error."
        success = False
    elif status == 'SUP':
        if not qflag:
            print "Anchor rules are loaded."
    elif status == 'SDN':
        if not qflag:
            print "Anchor rules are not loaded."
    elif status == 'EADDR':
        if not qflag:
            print "Error resolving host", server
        success = False
    else:
        # We should never reach here
        if not qflag:
            if status == 'NAK':
                print "Server did not understand command ", command
            else:
                print "do_command returned unknown response '", status, "'"
        raise AssertionError

    if (success):
        return 0
    else:
        return 1

if __name__ == "__main__":
    sys.exit(main())
