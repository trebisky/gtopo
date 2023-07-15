#!/bin/python

import socket
import sys
import time

gpx_path = "carrie.gpx"

server = ('localhost',5555)

def send_one_mark ( rcmd ) :
    s = socket.create_connection ( server )
    cmd = rcmd.encode()
    s.sendall ( cmd )
    reply = s.recv(16)
    #print ( reply )

def send_marks ( data ) :
    s = socket.create_connection ( server )

    #for line in data :
    #for i in range(1100,1174) :
    for i in range(len(data)) :
        print ( i )
        cmd = data[i].encode()
        s.sendall ( cmd )
        # We don't do anything with the reply
        # but it is important to wait for it.
        reply = s.recv(16)
        time.sleep ( 0.1 )
    s.close ()

def redo ( line ) :
    line = line.replace ( '>', '' )
    line = line.replace ( '"', '' )
    w = line.split()
    lat = w[1].replace ( 'lat=', '' )
    long = w[2].replace ( 'lon=', '' )
    return "MC " + long + " " + lat

def gpx_read ( path ) :
    file = open(path)
    rv = []
    for rline in file:
        line = rline.strip()
        if "trkpt " in line :
            rv.append ( redo ( line ) )
    return rv

data = gpx_read ( gpx_path )

if ( len(sys.argv) > 1 ) :
    x = sys.argv[1]
    i = int ( x )
    print ( i )
    send_one_mark ( data[i] )
    sys.exit()

#for l in data :
#    print ( l )

#print ( len(data) )
# 1175 items in original data

send_marks ( data )

# THE END
