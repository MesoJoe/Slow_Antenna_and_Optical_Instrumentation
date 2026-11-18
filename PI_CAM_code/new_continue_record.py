#!/usr/bin/env python3
import os
import time
import picamera
import gpsd
import datetime as dt

# Connect to the gps 
gpsd.connect()

file_directory = '/home/pi/Desktop/'

while True:
    file_list = os.listdir(file_directory)
    h264_files = [f for f in file_list if f.endswith('.h264') and f[-8:-5].isdigit()]
    
    if h264_files:
        counts = [int(f[-8:-5]) for f in h264_files]
        count = max(counts) + 1
    else:
        count = 0

    # camera settings
    with picamera.PiCamera() as camera:
        camera.resolution = (640, 480)
        camera.framerate = 90
        camera.annotate_frame_num = True
        camera.annotate_text_size = 10
        camera.annotate_background = picamera.Color('black') 
        time.sleep(1)  

        
        
        
        ### for annotating time on frame, uses system time if GPS is not avail 
        start = dt.datetime.now()
        try:
            packet = gpsd.get_current()
            if packet.time:
                startstring = packet.time
                annotation = '{}  Lat: {}  Lon: {}'.format(packet.time, packet.lat, packet.lon)
            else:
                startstring = start.strftime('%Y%m%d_%H%M%S')
                annotation = '{}'.format(start.strftime('%Y-%m-%d %H:%M:%S'))
        except:
            startstring = start.strftime('%Y%m%d_%H%M%S')
            annotation = '{}'.format(start.strftime('%Y-%m-%d %H:%M:%S'))

        
        camera.annotate_text = annotation

        
        filename = '{}/{}_{}.h264'.format(file_directory, str(count).zfill(3), startstring)
        camera.start_recording(filename)

        try:            
            while (dt.datetime.now() - start).seconds < 300:
                try:
                    packet = gpsd.get_current()
                    if packet.time:
                        annotation = '{}  Lat: {}  Lon: {}'.format(packet.time, packet.lat, packet.lon)
                    else:
                        annotation = '{}'.format(dt.datetime.now().strftime('%Y-%m-%d %H:%M:%S'))
                except:
                    annotation = '{}'.format(dt.datetime.now().strftime('%Y-%m-%d %H:%M:%S'))

                camera.annotate_text = annotation
                camera.wait_recording(0.4)
        finally:
            camera.stop_recording()

