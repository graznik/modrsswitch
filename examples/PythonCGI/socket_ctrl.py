#!/usr/bin/python

# Import modules for CGI handling
import os
import cgi
import cgitb
import syslog

sock_file = os.path.abspath('/dev/rsswitch')
sock_data = {'off': 0, 'on': 1}
dev = 0  # Only socket type 0 supported at the moment

form = cgi.FieldStorage()

if "sock_group0" in form:
        group = 0
        if form.getvalue('group0'):
                socket = int(form.getvalue('group0'))
        else:
                socket = "Not entered"

        if form.getvalue('sock_group0'):
                data = int(sock_data[form.getvalue('sock_group0')])
        else:
                data = "Not set"

elif "sock_group1" in form:
        group = 1
        if form.getvalue('group1'):
                socket = int(form.getvalue('group1'))
        else:
                socket = "Not entered"

        if form.getvalue('sock_group1'):
                data = int(sock_data[form.getvalue('sock_group1')])
        else:
                data = "Not set"

msg = "%d%d%d%d" % (0, group, socket, data)
with open(sock_file, 'w') as f:
        try:
                syslog.syslog(msg)
                f.write(msg)
        except:
                print "Status: 500"
                cgitb.handler()

print "Content-type:text/html\r\n\r\n"
print "<html>"
print "<head>"
print "<title>Remote socket control</title>"
print "</head>"
print "<body>"
print "<h2> Selected socket group is %s</h2>" % group
print "<h2> Selected socket is %s</h2>" % socket
print "<h2> Selected data is %s</h2>" % data
print "</body>"
print "</html>"
