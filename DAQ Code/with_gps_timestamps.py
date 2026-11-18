# -*- coding: utf-8 -*-
"""
Created on Thu Aug  8 11:15:30 2024

@author: jmb84
"""

from daqhats import mcc128, OptionFlags, HatIDs, HatError
from daqhats_utils import (
    select_hat_device,
    chan_list_to_mask,
    AnalogInputMode,
    AnalogInputRange
)
import h5py
import os
import time
from datetime import datetime
import gpsd  

SCAN_RATE = 20000          # 20,000 samples/second 
SAMPLES_PER_CHANNEL = 20000
FILE_DURATION = 1 *  60 # 1 minute

def main():
    channels = [2]  # Read from channel 2
    channel_mask = chan_list_to_mask(channels)

    # Initialize GPS 
    init_gps()

    try:
        address = select_hat_device(HatIDs.MCC_128)
        hat = mcc128(address)

        # Single-ended input, ±10 V range
        hat.a_in_mode_write(AnalogInputMode.SE)
        hat.a_in_range_write(AnalogInputRange.BIP_10V)

        print(f'Selected MCC 128 HAT device at address {address}')

        hat.a_in_scan_start(
            channel_mask,
            SAMPLES_PER_CHANNEL,
            SCAN_RATE,
            OptionFlags.CONTINUOUS
        )

        read_voltage_data(hat)

    except (HatError, ValueError) as err:
        print('\n', err)


def init_gps():
    """Connect to gpsd daemon (no data request yet)."""
    try:
        gpsd.connect()
        print("Connected to gpsd.")
    except Exception as e:
        print(f"Error connecting to GPSD: {e}")


def read_voltage_data(hat):
    basepath = '/home/pi/Documents/Measurement_Computing'
    os.makedirs(basepath, exist_ok=True)

    start_time = time.time()
    file_index = 0
    file_path = get_new_file_path(basepath, file_index)

    print(file_path)

    with h5py.File(file_path, 'w') as h5file:

        # Voltage data will be interleaved:
        # Ch0, Ch2, Ch0, Ch2, ...
        dataset_voltage = h5file.create_dataset(
            'voltages',
            (0,),
            maxshape=(None,),
            chunks=True,
            dtype='f'
        )

        dataset_gps = h5file.create_dataset(
            'gps_timestamp',
            (0,),
            maxshape=(None,),
            chunks=True,
            dtype='S20'
        )

        while True:
            read_result = hat.a_in_scan_read_numpy(
                SAMPLES_PER_CHANNEL,
                timeout=1.0
            )

            if read_result.hardware_overrun or read_result.buffer_overrun:
                print('\n\nError: Overrun detected\n')
                break

            gps_timestamp = get_gps_timestamp()

            if read_result.data.size > 0:

                # Append voltage data
                current_size = dataset_voltage.shape[0]
                dataset_voltage.resize(
                    (current_size + read_result.data.shape[0],)
                )
                dataset_voltage[current_size:] = read_result.data

                # Append GPS timestamp for every voltage sample
                current_size = dataset_gps.shape[0]
                dataset_gps.resize(
                    (current_size + read_result.data.shape[0],)
                )
                dataset_gps[current_size:] = (
                    [gps_timestamp.encode('utf-8')]
                    * read_result.data.shape[0]
                )

            # Start a new file after FILE_DURATION seconds
            if time.time() - start_time >= FILE_DURATION:
                file_index += 1
                file_path = get_new_file_path(basepath, file_index)

                h5file.close()
                h5file = h5py.File(file_path, 'w')

                dataset_voltage = h5file.create_dataset(
                    'voltages',
                    (0,),
                    maxshape=(None,),
                    chunks=True,
                    dtype='f'
                )

                dataset_gps = h5file.create_dataset(
                    'gps_timestamp',
                    (0,),
                    maxshape=(None,),
                    chunks=True,
                    dtype='S20'
                )

                start_time = time.time()
                print(f"Started new file: {file_path}")


def get_gps_timestamp():
    """Get current GPS timestamp or return 'N/A' if unavailable."""
    try:
        gps_data = gpsd.get_current()

        if gps_data.mode >= 2:  # 2D or 3D fix
            return gps_data.time
        else:
            return "N/A"

    except Exception as e:
        print(f"Error fetching GPS data: {e}")
        return "N/A"


def get_new_file_path(basepath, index):
    """Generate new file path with system timestamp and index."""
    timestamp = datetime.now().strftime('%Y%m%d_%H%M%S.%f')
    return os.path.join(
        basepath,
        f'voltage_data_{timestamp}_{index}.h5'
    )


if __name__ == '__main__':
    main()
