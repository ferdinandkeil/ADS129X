"""
waveout.py - Writes EMG readings to a file.

Copyright (c) 2014 Ferdinand Keil

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
"""

import serial, argparse
import wave
import struct
import time
import fourbfiveb
import os, sys
import csv
import h5py

class LostSyncException(Exception):
  pass

def decodePacket(x):
  if len(x) != 5:
    raise LostSyncException('string too short: needs 5 byte')
  if x[0]&0xF8 != 0:
    #print " ".join("{:02x}".format(c) for c in x)
    raise LostSyncException('first byte not sync byte')
  nibbles = (
    fourbfiveb.decode(((x[0]<<2) | (x[1]>>6))&0x1F), # nibble 0
    fourbfiveb.decode((x[1]>>1)&0x1F),               # nibble 1
    fourbfiveb.decode(((x[1]<<4) | (x[2]>>4))&0x1F), # nibble 2
    fourbfiveb.decode(((x[2]<<1) | (x[3]>>7))&0x1F), # nibble 3
    fourbfiveb.decode((x[3]>>2)&0x1F),               # nibble 4
    fourbfiveb.decode(((x[3]<<3) | (x[4]>>5))&0x1F), # nibble 5
    fourbfiveb.decode(x[4]&0x1F)                     # nibble 6
  )
  value = (nibbles[1]<<20) | (nibbles[2]<<16) | \
          (nibbles[3]<<12) | (nibbles[4]<<8)  | \
          (nibbles[5]<<4)  | (nibbles[6])
  # convert to signed int
  if value & 0x800000:
    value = -0x1000000 + value
  return (nibbles[0], value) # (channel, value)

# main() function
def main():
  # create parser
  parser = argparse.ArgumentParser(description="""Writes EMG readings to a file.

                                                  Output gets written to OUT. The format is determined by the file's extension.
                                                  Possible formats are:
                                                    wave file (.wav),
                                                    CSV file (csv or .txt) and
                                                    HDF5 file (.h5 or .hdf5).
                                                  The SAMPLERATE has to be the same value that is set in the firmware.""")
  # add expected arguments
  parser.add_argument('-p', '--port', dest='port', required=True)
  parser.add_argument('out', help='file to write the output to')
  parser.add_argument('-s', '--samplerate', dest='samplerate', type=int, required=True)

  # parse args
  args = parser.parse_args()

  strPort = args.port
  outFile = args.out
  samplerate = int(args.samplerate)

  # check output format
  _, extension = os.path.splitext(outFile)
  outputFormat = ''
  if extension == '.wav':
    outputFormat = 'wave'
  elif extension in ('.h5', '.hdf5'):
    outputFormat = 'hdf5'
  elif extension in ('.csv', '.txt'):
    outputFormat = 'csv'
  elif extension == '.raw':
    outputFormat = 'raw'
  else:
    print 'Output format is not supported: %s' % extension
    sys.exit(0)

  print('reading from serial port %s...' % strPort)
  ser = serial.Serial(strPort, 12000000)

  rawData = bytearray()
  start = 0
  try:
    start = time.clock()
    while (True):
      rawData.extend(ser.read(ser.inWaiting() or 10000)) # read bytes
  except KeyboardInterrupt:
    stop = time.clock()
    print("...completed reading data.")

    # align to packet header
    startPacket = 0
    while rawData[startPacket]&0xF8 != 0:
      startPacket += 1
    rawData = rawData[startPacket:]
    # align to channel 1 data
    startFirstChannel = 0
    channel = 0
    while channel != 1:
      channel = decodePacket(rawData[startFirstChannel:startFirstChannel+5])[0]
      startFirstChannel += 5
    rawData = rawData[startFirstChannel-5:]

    if outputFormat != 'raw':
      # extract samples from raw data
      nrRawFrames = int(len(rawData)/40) # 40 bit/frame
      nrFrames = 0
      samples = []
      index = 0
      droppedBytes = 0
      while index < len(rawData)-40:
        frame = rawData[index:index+40]
        row = []
        try:
          for channel in range(8):
            _channel,sample = decodePacket(frame[5*channel:5*channel+5])
            if _channel != channel+1:
              raise LostSyncException()
            if outputFormat == 'wave':
              row.append(struct.pack('<i', sample)[0:3])
            elif outputFormat == 'csv':
              row.append(sample)
            elif outputFormat == 'hdf5':
              row.append(sample)
        except LostSyncException:
          index += 1
          droppedBytes += 1
          # realign index pointer
          while rawData[index]&0xF8 != 0 or decodePacket(rawData[index:index+5])[0] != 1:
            index += 1
            droppedBytes += 1
        else:
          # append one row
          if outputFormat == 'wave':
            samples.extend(row)
          elif outputFormat == 'csv':
            samples.append(row)
          elif outputFormat == 'hdf5':
            samples.append(row)
          nrFrames += 1
          index += 40

    # write data to file
    if outputFormat == 'wave':
      wavefile = wave.open(outFile, 'w')
      wavefile.setnchannels(8)
      wavefile.setframerate(samplerate)
      wavefile.setsampwidth(3)
      wavefile.writeframes(b"".join(samples))
      wavefile.close()
    elif outputFormat == 'csv':
      csvfile = open(outFile, 'wb')
      csvwriter = csv.writer(csvfile)
      csvwriter.writerows(samples)
      csvfile.close()
    elif outputFormat == 'raw':
      with open(outFile, 'wb') as rawfile:
        rawfile.write(rawData)
    elif outputFormat == 'hdf5':
      hdf5file = h5py.File(outFile, 'wb')
      sampleSet = hdf5file.create_dataset("samples", (len(samples), 8), dtype="i")
      sampleSet[:] = samples
      hdf5file.flush()
      hdf5file.close()


  print("wrote output to %s" % outFile)
  print("recorded %f seconds of data." % (stop-start))
  print("recorded %u samples (%f sps)." % (nrFrames, nrFrames/(stop-start)))
  print("dropped %u bytes." % droppedBytes)
  print('finished.')


# call main
if __name__ == '__main__':
  main()
