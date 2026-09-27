"""Independent exact numerical checks and deterministic input recipe verification."""
from __future__ import annotations
import contextlib, ctypes, hashlib, math, mmap, os, struct
CAP=12*1024**3
def sha(path):
 h=hashlib.sha256()
 with open(path,"rb") as f:
  for b in iter(lambda:f.read(1024**2),b""):h.update(b)
 return h.hexdigest()

class RefValue(ctypes.Structure):_fields_=[('expected',ctypes.c_double),('abs_sum',ctypes.c_double),('terms',ctypes.c_uint64),('flags',ctypes.c_uint32),('reserved',ctypes.c_uint32)]
class Projection(ctypes.Structure):_fields_=[('expected',ctypes.c_double),('observed',ctypes.c_double),('abs_sum',ctypes.c_double),('projection_error',ctypes.c_double),('terms',ctypes.c_uint64),('flags',ctypes.c_uint32),('reserved',ctypes.c_uint32)]
class Reference:
 def __init__(self,library):
  self.lib=ctypes.CDLL(str(library));base=[ctypes.c_uint32]+[ctypes.c_uint64]*3+[ctypes.c_void_p]*3
  self.lib.matrix_reference_dot.argtypes=base+[ctypes.c_uint64]*2+[ctypes.POINTER(RefValue)]
  self.lib.matrix_reference_full.argtypes=base+[ctypes.POINTER(RefValue),ctypes.c_uint64,ctypes.c_uint64]
  self.lib.matrix_reference_projection.argtypes=base+[ctypes.c_void_p,ctypes.c_void_p,ctypes.POINTER(Projection),ctypes.c_uint64]
  for n in ['dot','full','projection']:getattr(self.lib,'matrix_reference_'+n).restype=ctypes.c_int
 @staticmethod
 def target_bound(e,k,absolute,outputs=1):
  if e in (4,101):return 0.
  from fractions import Fraction
  precision=53 if e in (2,9) else 24;emin=1074 if e in (2,9) else 149;q=2*k+1;qu=Fraction(q,1<<precision)
  if qu>=1 or outputs<1:raise ValueError('unsupported gamma/domain')
  # Conservative gradual-underflow term for every multiplication/addition and
  # every contributing output dot. Exact construction, then round outward.
  bound=(qu*Fraction(absolute)+Fraction(q*outputs,1<<emin))/(1-qu)
  result=float(bound)
  if not math.isfinite(result):raise ValueError('unrepresentable error bound')
  return math.nextafter(result,math.inf) if Fraction(result)<bound else result
 def adjudicate(self,case,paths,output,full_limit=50_000_000):
  e,m,n,k=(case[x] for x in ('endpoint','m','n','k'));osize=8 if e in (2,9) else 1 if e==101 else 4
  insize=8 if e in (2,9) else 2 if e in (3,100) else 1 if e in (4,101) else 4
  lengths=[m*k*insize,k*n*insize,m*n*osize,m*n*osize]
  scalar=m*n*(k+(e in (1,5,8)))<=full_limit;refbytes=(32*m*n if scalar else 48*m+24*k+n)+16*1024**2
  if sum(lengths)+refbytes+case.get("worker_live_bytes",0)>CAP:raise ValueError('reference static owned budget')
  with contextlib.ExitStack() as stack:
   ptrs=[];maps=[]
   for p,length in zip([*paths,output],lengths):
    f=stack.enter_context(open(p,'rb'))
    if os.fstat(f.fileno()).st_size!=length:raise ValueError('serialization length')
    mm=stack.enter_context(mmap.mmap(f.fileno(),0,access=mmap.ACCESS_COPY));maps.append(mm)
    # Save address without retaining a ctypes exported buffer beyond this iteration.
    ptrs.append(ctypes.addressof(ctypes.c_char.from_buffer(mm)))
   args=[e,m,n,k,*ptrs[:3]];raw=maps[-1];fmt='<d' if e in (2,9) else '<i' if e==4 else '<b' if e==101 else '<f'
   def observed(i,j):return struct.unpack_from(fmt,raw,osize*(i+j*m if e in (1,6,8) else i*n+j))[0]
   # Whole-output finite scan is independent of worker claims, including un-sampled entries.
   if any(not math.isfinite(v[0]) for v in struct.iter_unpack(fmt,raw)):raise ValueError('nonfinite output')
   checked=0;maximum=0.;dot_records=[];worst=None;projection_records=[]
   def compare(i,j,v):
    nonlocal checked,maximum,worst
    if v.flags or not all(map(math.isfinite,[v.expected,v.abs_sum])):raise ValueError('invalid oracle result')
    actual=observed(i,j)
    # Exact subtraction of serialized floats prevents accidental under-rounding at boundary.
    from fractions import Fraction
    diff=abs(Fraction(actual)-Fraction(v.expected));bound=self.target_bound(e,k,v.abs_sum)
    if e not in (4,101):bound=math.nextafter(bound+math.ulp(v.expected),math.inf)
    if diff>Fraction(bound):raise ValueError(f'numerical mismatch at {i},{j}: {actual} vs {v.expected}, bound {bound}')
    checked+=1;maximum=max(maximum,float(diff))
    record={'i':i,'j':j,'expected_hex':float(v.expected).hex(),'observed_hex':float(actual).hex(),'abs_error_hex':float(diff).hex(),'bound_hex':float(bound).hex()}
    if len(dot_records)<66:dot_records.append(record)
    if worst is None or float(diff)>float.fromhex(worst['abs_error_hex']):worst=record
   if scalar:
    values=(RefValue*(m*n))();rc=self.lib.matrix_reference_full(*args,values,m*n,full_limit)
    if rc:raise ValueError('full oracle status '+str(rc))
    for i in range(m):
     for j in range(n):compare(i,j,values[i*n+j])
    return {'method':'EXACT_FULL_DOT_ORACLE','checked_entries':checked,'total_entries':m*n,'max_abs_error':maximum,'audit':{'dots':dot_records,'worst_dot':worst,'projections':projection_records},'coverage':'FULL_OUTPUT_WITH_FROZEN_ROUNDING_BOUND'}
   if case['pattern'] in (0,1,2,3,5):
    if [sha(p) for p in paths]!=expected_input_hashes(case):raise ValueError('structured input recipe mismatch')
    from fractions import Fraction
    pattern=case['pattern'];has_beta=e in (1,5,8)
    for i in range(m):
     for j in range(n):
      expected=0 if pattern==0 else k if pattern==1 else int(i==j and i<k) if pattern==2 else (-k if (i+j+1)&1 else k) if pattern==3 else j%7-3
      if has_beta and case.get('c0_mode','nonzero')=='nonzero':expected+=Fraction(((i+2*j)%7)-3,8)
      if e==101:expected=max(-128,min(127,expected))
      actual=observed(i,j)
      # These integer/dyadic structured values are exactly representable over
      # admitted K<=25600 and C0 eighths; require complete exact equality.
      if Fraction(actual)!=expected:raise ValueError('structured full-output mismatch')
    return {'method':'ANALYTIC_STRUCTURED_FULL_OUTPUT','checked_entries':m*n,'total_entries':m*n,'coverage':'EXACT_FULL_STRUCTURED_OUTPUT','input_precondition':'caller independently verifies actual A/B/C0 against the bound generator recipe before adjudication'}
   # Fixed indices selected before reading output. No performance-dependent choice.
   indices={0,m*n-1}
   for r in range(64):indices.add(int.from_bytes(hashlib.sha256(f'\x50\x30\x34-fixed-dot/1701/{m}/{n}/{r}'.encode()).digest()[:8],'little')%(m*n))
   for index in sorted(indices):
    i,j=divmod(index,n);v=RefValue();rc=self.lib.matrix_reference_dot(*args,i,j,ctypes.byref(v))
    if rc:raise ValueError('sampled oracle status '+str(rc))
    compare(i,j,v)
   if e==101:raise ValueError('large saturated random output requires structured full checks; sampled-only cannot qualify')
   signs=(ctypes.c_int8*n)();values=(Projection*m)()
   for repeat in range(16):
    for j in range(n):signs[j]=1 if hashlib.sha256(f'\x50\x30\x34-projection/1702/{repeat}/{j}'.encode()).digest()[0]&1 else -1
    rc=self.lib.matrix_reference_projection(*args,ptrs[3],signs,values,m)
    if rc:raise ValueError('projection status '+str(rc))
    worst_projection=None
    for row,v in enumerate(values):
     if v.flags or not all(map(math.isfinite,[v.expected,v.observed,v.abs_sum,v.projection_error])):raise ValueError('projection finite/status')
     from fractions import Fraction
     limit=math.nextafter(self.target_bound(e,k,v.abs_sum,n)+v.projection_error,math.inf)
     error=abs(Fraction(v.observed)-Fraction(v.expected))
     if error>Fraction(limit):raise ValueError('whole-output projection mismatch')
     rec={'repeat':repeat,'row':row,'expected_hex':v.expected.hex(),'observed_hex':v.observed.hex(),'abs_error_hex':float(error).hex(),'bound_hex':limit.hex()}
     if worst_projection is None or float(error)>float.fromhex(worst_projection['abs_error_hex']):worst_projection=rec
    projection_records.append({'rows_checked':m,'worst':worst_projection})
   return {'method':'FIXED_SAMPLED_EXACT_DOTS_AND_16_WHOLE_OUTPUT_PROJECTIONS','checked_entries':checked,'total_entries':m*n,'max_abs_error':maximum,'audit':{'dots':dot_records,'worst_dot':worst,'projections':projection_records},'coverage':'BOUNDED_CHECKS_NOT_EXHAUSTIVE','false_accept_probability':'No unconditional floating-point probability guarantee for fixed public vectors and tolerance; exact random-sign ideal bound does not apply directly.'}

def expected_inputs(case):
 """Independent Python generator for bounded tests; physical byte layout explicit."""
 e,m,n,k,seed,pattern=[case[x] for x in ['endpoint','m','n','k','seed','pattern']];mask=(1<<64)-1
 if max(m*k,k*n,m*n)>100000:raise ValueError("bounded in-memory generator; use streaming expected_input_hashes")
 def next_value(matrix,i,j):
  nonlocal seed
  seed=(seed+0x9e3779b97f4a7c15)&mask;z=seed;z=((z^(z>>30))*0xbf58476d1ce4e5b9)&mask;z=((z^(z>>27))*0x94d049bb133111eb)&mask;z^=z>>31;v=z%255-127
  if pattern==0:return 0
  if pattern==1:return 1
  if pattern==2:return int(i==j)
  if pattern==3:return -1 if (i+j+matrix)&1 else 1
  if pattern==5:return (1 if j==0 else 0) if matrix==0 else ((j%7-3) if i==0 else 0)
  return v if e in (4,101) else v/128
 def pack_input(v):
  if e==100:
   bits=struct.unpack('<I',struct.pack('<f',v))[0];return struct.pack('<H',(bits+32767+((bits>>16)&1))>>16)
  return struct.pack('<d' if e in (2,9) else '<e' if e==3 else '<b' if e in (4,101) else '<f',v)
 a=[None]*(m*k);b=[None]*(n*k);c=[None]*(m*n);col=e in (1,6,8)
 for i in range(m):
  for t in range(k):a[i+t*m if col else i*k+t]=pack_input(next_value(0,i,t))
 for t in range(k):
  for j in range(n):b[t+j*k if col else t*n+j]=pack_input(next_value(1,t,j))
 fmt='<d' if e in (2,9) else '<i' if e==4 else '<b' if e==101 else '<f'
 for i in range(m):
  for j in range(n):c[i+j*m if col else i*n+j]=struct.pack(fmt,0 if case.get('c0_mode','nonzero')=='zero' else (i+2*j)%7-3 if e in (4,101) else ((i+2*j)%7-3)/8)
 return [b''.join(x) for x in (a,b,c)]

def expected_input_hashes(case):
 """Independent physical-order streaming generator, <=64KiB transient payload.
 SplitMix state jumps to each logical index, so column layout needs no matrix copy.
 """
 e,m,n,k,seed,pattern=[case[x] for x in ['endpoint','m','n','k','seed','pattern']];mask=(1<<64)-1;col=e in (1,6,8)
 def value(matrix,i,j,ordinal):
  z=(seed+0x9e3779b97f4a7c15*(ordinal+1))&mask;z=((z^(z>>30))*0xbf58476d1ce4e5b9)&mask;z=((z^(z>>27))*0x94d049bb133111eb)&mask;z^=z>>31
  if pattern==0:return 0
  if pattern==1:return 1
  if pattern==2:return int(i==j)
  if pattern==3:return -1 if (i+j+matrix)&1 else 1
  if pattern==5:return int(j==0) if matrix==0 else (j%7-3 if i==0 else 0)
  return z%255-127 if e in (4,101) else (z%255-127)/128
 results=[]
 for matrix,count in [(0,m*k),(1,k*n),(2,m*n)]:
  h=hashlib.sha256();chunk=bytearray()
  for index in range(count):
   if matrix==0:i,j=(index%m,index//m) if col else divmod(index,k);v=value(0,i,j,i*k+j)
   elif matrix==1:i,j=(index%k,index//k) if col else divmod(index,n);v=value(1,i,j,m*k+i*n+j)
   else:
    i,j=(index%m,index//m) if col else divmod(index,n);v=(i+2*j)%7-3 if e in (4,101) else ((i+2*j)%7-3)/8
   if matrix==2 and case.get('c0_mode','nonzero')=='zero':v=0
   if matrix!=2 and e==100:
    bits=struct.unpack('<I',struct.pack('<f',v))[0];encoded=struct.pack('<H',(bits+32767+((bits>>16)&1))>>16)
   else:
    fmt='<d' if e in (2,9) else '<e' if matrix!=2 and e==3 else '<b' if (matrix!=2 and e in (4,101)) or e==101 else '<i' if matrix==2 and e==4 else '<f';encoded=struct.pack(fmt,v)
   chunk.extend(encoded)
   if len(chunk)>=65536:h.update(chunk);chunk.clear()
  h.update(chunk);results.append(h.hexdigest())
 return results

def planned_combined_bytes(case,scratch_allowance):
 """Prospective whole controller+one-worker owned lifetime; no host-RAM query."""
 e,m,n,k=[case[x] for x in ['endpoint','m','n','k']];si=8 if e in (2,9) else 2 if e in (3,100) else 1 if e in (4,101) else 4;so=8 if e in (2,9) else 1 if e==101 else 4
 a=m*k*si;b=k*n*si;c=m*n*so;full=m*n*(k+(e in (1,5,8)))<=50_000_000
 result=2*(a+b+2*c)+1024+scratch_allowance+(32*m*n if full else 48*m+24*k+n)+16*1024**2+65536
 if min(m,n,k)<=0 or result>CAP:raise ValueError('combined planned own allocation exceeds cap')
 return result
