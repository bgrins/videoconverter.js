"""
 *  Copyright (c) 2012 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
"""
#!/usr/bin/env python
import sys,string,os,re,math,numpy
scale = 2**16
def dist(p1,p2):
  x1,y1 = p1
  x2,y2 = p2
  if x1==x2 and y1==y2 :
    return 1.0 
  return 1/ math.sqrt((x1-x2)*(x1-x2)+(y1-y2)*(y1-y2))

def gettaps(p):
  def l(b):
    return int(math.floor(b))
  def h(b):
    return int(math.ceil(b))
  def t(b,p,s):
    return int((scale*dist(b,p)+s/2)/s)
  r,c = p
  ul=[l(r),l(c)]
  ur=[l(r),h(c)]
  ll=[h(r),l(c)]
  lr=[h(r),h(c)]
  sum = dist(ul,p)+dist(ur,p)+dist(ll,p)+dist(lr,p)
  t4 = scale - t(ul,p,sum) - t(ur,p,sum) - t(ll,p,sum);
  return [[ul,t(ul,p,sum)],[ur,t(ur,p,sum)],
          [ll,t(ll,p,sum)],[lr,t4]]

def print_mb_taps(angle,blocksize):
  theta = angle / 57.2957795;
  affine = [[math.cos(theta),-math.sin(theta)],
            [math.sin(theta),math.cos(theta)]]
  radius = (float(blocksize)-1)/2
  print " // angle of",angle,"degrees"
  for y in range(blocksize) :
    for x in range(blocksize) :
      r,c = numpy.dot(affine,[y-radius, x-radius])
      tps = gettaps([r+radius,c+radius])
      for t in tps :
        p,t = t
        tr,tc = p
        print " %2d, %2d, %5d, " % (tr,tc,t,),
      print " // %2d,%2d " % (y,x)

i=float(sys.argv[1])
while  i <= float(sys.argv[2]) :
  print_mb_taps(i,float(sys.argv[4]))
  i=i+float(sys.argv[3])
"""

taps = []
pt=dict()
ptr=dict()
for y in range(16) :
  for x in range(16) :
    r,c = numpy.dot(affine,[y-7.5, x-7.5])
    tps = gettaps([r+7.5,c+7.5])
    j=0
    for tp in tps : 
      p,i = tp
      r,c = p
      pt[y,x,j]= [p,i]
      try: 
        ptr[r,j,c].append([y,x])
      except:
        ptr[r,j,c]=[[y,x]]
      j = j+1 

for key in sorted(pt.keys()) :
  print key,pt[key]

lr = -99
lj = -99 
lc = 0

shuf=""
mask=""
for r,j,c in sorted(ptr.keys()) :
  for y,x in ptr[r,j,c] :
    if lr != r or lj != j :
      print "shuf_"+str(lr)+"_"+str(lj)+"_"+shuf.ljust(16,"0"), lc
      shuf=""
      lc = 0
    for i in range(lc,c-1) :
      shuf = shuf +"0"
    shuf = shuf + hex(x)[2]
    lc =c
    break
  lr = r
  lj = j
#  print r,j,c,ptr[r,j,c]    
#  print 

for r,j,c in sorted(ptr.keys()) :
  for y,x in ptr[r,j,c] :
    print r,j,c,y,x 
    break
"""
