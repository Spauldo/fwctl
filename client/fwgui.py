#!/usr/bin/env python2

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
import fwclient
import wx

server = 'bart.spauldo.net'
port = 2287
poll_interval = 5000 # How often to check the server, in milliseconds


def print_help():
    print "fwgui.py <-s server_address> <-p port> [-h]"


class Systray_Icon(wx.TaskBarIcon):
    ID_MENU_UP     = wx.NewId()
    ID_MENU_DOWN   = wx.NewId()
    ID_MENU_CLOSE  = wx.NewId()
    ID_CHECK_TIMER = wx.NewId()

    server = 'bart.spauldo.net'
    port = 2287

    def __init__(self):
        super(Systray_Icon, self).__init__()
        self.server = ''
        self.port = 0

        self.set_icon('err', "Waiting to connect to server.")

        self.check_timer = wx.Timer(self, self.ID_CHECK_TIMER)

        self.Bind(wx.EVT_MENU, self.bring_up_firewall, id=self.ID_MENU_UP)
        self.Bind(wx.EVT_MENU, self.bring_down_firewall, id=self.ID_MENU_DOWN)
        self.Bind(wx.EVT_MENU, self.cleanup_and_exit, id=self.ID_MENU_CLOSE)
        self.Bind(wx.EVT_TIMER, self.get_status, self.check_timer)

    def start(self, server_address, port_num):
        self.server = server_address
        self.port = port_num
        self.get_status()
        self.check_timer.Start(poll_interval)

    def set_icon(self, state, message):
        if state == 'up':
            icon = wx.IconFromBitmap(wx.Bitmap('fwup16.png'))
        elif state == 'down':
            icon = wx.IconFromBitmap(wx.Bitmap('fwdown16.png'))
        else:
            icon = wx.IconFromBitmap(wx.Bitmap('fwerr16.png'))

        self.SetIcon(icon, message)

    def CreatePopupMenu(self):
        menu = wx.Menu()
        menu.Append(self.ID_MENU_UP, "Bring Up Firewall")
        menu.Append(self.ID_MENU_DOWN, "Bring Down Firewall")
        menu.AppendSeparator()
        menu.Append(self.ID_MENU_CLOSE, "Close App")
        return menu

    def get_status(self, event=None):
        status = fwclient.do_command(self.server, self.port, 'FST')
        if status == 'SUP':
            self.set_icon('up', "Firewall is up.")
        elif status == 'SDN':
            self.set_icon('down', "Firewall is down.")
        elif status == 'EADDR':
            self.set_icon('err', "Can't find server '", self.server, "'")
        else:
            print "Error:", status
            self.set_icon('err', "Error communicating with server.")

    def bring_up_firewall(self, event=None):
        status = fwclient.do_command(self.server, self.port, 'FUP')
        if status == 'EADDR':
            self.set_icon('err', "Can't find server '", self.server, "'")
        elif status != 'ACK':
            self.set_icon('err', "Error communicating with server.")
        else:
            self.get_status()

    def bring_down_firewall(self, event=None):
        status = fwclient.do_command(self.server, self.port, 'FDN')
        if status == 'EADDR':
            self.set_icon('err', "Can't find server '", self.server, "'")
        elif status != 'ACK':
            self.set_icon('err', "Error communicating with server.")
        else:
            self.get_status()

    def cleanup_and_exit(self, event=None):
        self.check_timer.Stop()
        self.RemoveIcon()
        self.Destroy()
        # Required because we're using a null top level window.  I'm open to
        # suggestions for something better.
        sys.exit()


class App(wx.App):
    icon = None

    def OnInit(self):
        self.SetTopWindow(wx.Frame(None, -1))
        self.icon = Systray_Icon()
        return True


def main():
    global server
    global port

    try:
        options, remainder = getopt.getopt(sys.argv[1:], 's:p:h')
    except getopt.error:
        print_help()
        sys.exit()

    for opt, arg in options:
        if opt == '-s':
            server = arg
        elif opt == '-p':
            port = arg
        elif opt == '-h':
            print_help()
            sys.exit()

    if (server == ''):
        print_help()
        sys.exit()

    app = App()
    app.icon.start(server, port)
    app.MainLoop()


if __name__ == "__main__":
    main()
