import sockios
sockios.init()

iflist = sockios.get_iflist()

print 'network interfaces:'

for ifname in iflist:
    conf = sockios.get_ifconf(ifname)
    print "\t", ifname, 'is', ('UP' if sockios.is_up(ifname) else 'DOWN'), \
            'and is addressed as', conf['in_addr']


