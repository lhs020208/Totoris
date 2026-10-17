"""Source-only sprite generator, uses numpy scipy pillow outside Unreal.
PNG results are already included in the fix package; running this is optional.
"""
import os
import numpy as np
from PIL import Image
from scipy.ndimage import gaussian_filter

ROOT=os.path.dirname(os.path.abspath(__file__))
OUT=os.path.join(ROOT, 'SourceArt','Anjanath')
os.makedirs(OUT,exist_ok=True)
N=512
y,x=np.mgrid[0:N,0:N].astype(np.float32)
x=(x+.5)/N*2-1
y=(y+.5)/N
rng=np.random.default_rng(32052026)

def smooth(a,b,z):
    q=np.clip((z-a)/(b-a),0,1)
    return q*q*(3-2*q)

def noise(scale):
    arr=rng.standard_normal((N,N)).astype(np.float32)
    arr=gaussian_filter(arr,scale,mode='reflect')
    return arr/(np.std(arr)+1e-6)

n0=noise(42); n1=noise(20); n2=noise(9); n3=noise(3)
side_noise=.55*n0+.31*n1+.13*n2
cent=.075*np.sin(8*y+0.8)+.034*np.sin(27*y+.8)+.019*n1
# Three offset overlapping tongues and a strong central spine
width=.035+.51*(y**1.33)
flame_radius=np.abs(x-cent)
edge=width*(1+0.12*side_noise)
outer=1-smooth(edge-.082,edge+.05,flame_radius)
outer*=smooth(.015,.17,y)*(1-smooth(.83,1.,y))

# Asymmetric smaller streams create the recognisable turbulent flame silhouette.
tongues=[]
for side,top,factor in [(-1,.26,.57),(1,.39,.50),(-1,.52,.30)]:
    yt=np.clip((y-top)/(1-top),0,1)
    cx=side*(.095+.31*yt)+.045*np.sin(19*y+side*1.9)
    w=.015+factor*.18*yt
    t=(1-smooth(w-.055,w+.055,np.abs(x-cx)))
    t*=smooth(top,top+.14,y)*(1-smooth(.89,.995,y))
    tongues.append(t)
outer=np.maximum.reduce([outer]+tongues)
# Highly detailed but softened wispy boundary; alpha always fades to 0 at edges
boundary=.84+.14*n1+.075*n2+.035*n3
alpha=np.clip(outer*boundary,0,1)
core=(1-smooth(.02, width*.70+.02,flame_radius))
core*=smooth(.30,.61,y)
inner=(1-smooth(.06,width*1.10,flame_radius))*(.58+.42*smooth(.18,.70,y))
heat=np.clip(.11+.63*inner+.39*core+.08*n2,0,1)
# Rich red outside, saturated orange, yellow-hot center with small pale hotspots
stops=np.array([[.78,.028,.008],[1.,.17,.012],[1.,.55,.045],[1.,.92,.42]],np.float32)
color=np.zeros((N,N,3),np.float32)
color[:]=stops[0]
for lo,hi,c0,c1 in [(0,.35,stops[0],stops[1]),(.35,.69,stops[1],stops[2]),(.69,1.,stops[2],stops[3])]:
    t=np.clip((heat-lo)/(hi-lo),0,1)[:,:,None]
    new=c0[None,None,:]*(1-t)+c1[None,None,:]*t
    select=(heat>=lo)&(heat<=hi)
    color[select]=new[select]
color[heat>1]=stops[-1]
# Small refractive/streaky internal modulation
flicker=np.clip(.92+.09*n1+.065*n2, .7,1.05)
color=np.clip(color*flicker[:,:,None],0,1)
rgba=np.uint8(np.clip(np.dstack((color,alpha))*255,0,255))
Image.fromarray(rgba,'RGBA').save(os.path.join(OUT,'T_AnjanathFire.png'),optimize=True)

# Smoke: multi-scale density field, feathery radial rim, self-shadowed internal streaks.
n0=noise(60);n1=noise(27);n2=noise(12);n3=noise(5)
warp_x=.10*n1+.042*n2
warp_y=.08*n0+.055*n1
qx=x+warp_x
qy=(y-.50)*1.55+warp_y
radius=np.sqrt(qx*qx+qy*qy)
cloud=1-smooth(.35,.98,radius+.09*n1+.047*n2)
detail=np.clip(.53+.15*n0+.16*n1+.085*n2+.043*n3,0,1)
alpha=np.clip(cloud*(.20+.63*detail),0,.76)
# Soft central billows and hazy outer edges
alpha*=1-smooth(.94,1.05,np.maximum(np.abs(x),np.abs((y-.5)*2)))
shade=np.clip(.065+.045*n1+.018*n2,.028,.15)
smoke_color=np.stack((shade*.83,shade*.88,shade),axis=-1)
rgba=np.uint8(np.clip(np.dstack((smoke_color,alpha))*255,0,255))
Image.fromarray(rgba,'RGBA').save(os.path.join(OUT,'T_AnjanathSmoke.png'),optimize=True)
for f in ('T_AnjanathFire.png','T_AnjanathSmoke.png'):
    print(f,os.path.getsize(os.path.join(OUT,f)))
