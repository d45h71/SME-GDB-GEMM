#!/usr/bin/env python3
"""Build native components without executing experiment workers."""
from __future__ import annotations
import argparse, datetime, fcntl, hashlib, json, os, platform, shutil, signal, subprocess, sys, time
from pathlib import Path
from common import machine_lock

ROOT = Path(__file__).resolve().parents[1]
COMPONENTS = ('matrix','stock','first','second','accelerate','libxsmm','kleidiai','openblas','compute','transfer','ablation')
def digest(p): return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def files_under(paths):
    out={}
    for p in paths:
        for q in sorted(p.rglob('*')) if p.is_dir() else [p]:
            if q.is_file():
                if q.is_symlink(): raise RuntimeError('source symlink prohibited: '+str(q))
                out[str(q.relative_to(ROOT))]=digest(q)
    return out

def toolchain():
    if platform.system()!='Darwin' or platform.machine()!='arm64': raise RuntimeError('Build requires Apple arm64 macOS and Xcode with SME2 assembler support.')
    def query(*args): return subprocess.check_output(args,text=True).strip()
    cc=Path(os.environ.get('CC') or query('xcrun','--find','clang')).absolute()
    cxx=Path(os.environ.get('CXX') or query('xcrun','--find','clang++')).absolute()
    sdk=Path(os.environ.get('SDKROOT') or query('xcrun','--show-sdk-path')).resolve()
    return dict(cc=str(cc),cxx=str(cxx),sdk=str(sdk),compiler_sha256=digest(cc),compiler_version=query(str(cc),'--version'),sdk_version=query('xcrun','--show-sdk-version'),python=sys.version,platform=platform.platform(),build_environment={k:os.environ.get(k) for k in ('CC','CXX','SDKROOT','CFLAGS','CXXFLAGS','CPPFLAGS','LDFLAGS','CPATH','C_INCLUDE_PATH','CPLUS_INCLUDE_PATH','LIBRARY_PATH','MACOSX_DEPLOYMENT_TARGET')})

class Builder:
    def __init__(self,build):
        self.build=build; self.tools=toolchain(); self.commands=[]
        self.build.mkdir(parents=True,exist_ok=True)
        self.logdir=build/'logs'/datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%S.%fZ');self.logdir.mkdir(parents=True)
        for folder in ('lib','bin','objects','tmp'): (build/folder).mkdir(exist_ok=True)
        self.env=os.environ.copy();self.env.update(TMPDIR=str(build/'tmp'),CLANG_MODULE_CACHE_PATH=str(build/'tmp/modules'),PYTHONDONTWRITEBYTECODE='1',SDKROOT=self.tools['sdk'],LC_ALL='C')
        self.cc=self.tools['cc'];self.cxx=self.tools['cxx'];self.sdk=self.tools['sdk']
        self.common=['-isysroot',self.sdk,'-O3','-fPIC','-fno-modules',f'-ffile-prefix-map={ROOT}=.',f'-ffile-prefix-map={build}=build']
        self.cpp=[*self.common,'-std=c++17','-march=armv8-a+nosve']
        self.asm=[*self.common,'-march=armv9-a+sme2+sme+sme-f64f64']
        self.include=['-I',str(ROOT/'src/include'),'-I',str(ROOT/'src/harness')]
        self.path=build/'build-manifest.json'
        try:self.manifest=json.loads(self.path.read_text())
        except FileNotFoundError:self.manifest={'schema':1,'components':{}}
        self.manifest['build_directory']=str(build)
    def save(self):
        self.manifest['source_files']={};self.manifest['output_files']={}
        for c in self.manifest['components'].values():
            if c.get('status')=='PASS':self.manifest['source_files'].update(c['source_files']);self.manifest['output_files'].update(c['output_files'])
        p=self.path.with_suffix('.tmp');p.write_text(json.dumps(self.manifest,indent=2)+'\n');p.replace(self.path)
    def run(self,argv,cwd=None,timeout=3600):
        argv=list(map(str,argv));idx=len(self.commands);row={'argv':argv,'cwd':str(cwd or ROOT),'started':time.time()};self.commands.append(row)
        try:
            with (self.logdir/f'{idx:04}.stdout').open('wb') as o,(self.logdir/f'{idx:04}.stderr').open('wb') as e:
                p=subprocess.Popen(argv,cwd=cwd or ROOT,env=self.env,stdout=o,stderr=e,start_new_session=True)
                try:p.wait(timeout=timeout)
                except BaseException:
                    if p.poll() is None:os.killpg(p.pid,signal.SIGKILL);p.wait()
                    raise
            row['returncode']=p.returncode
            if p.returncode:raise RuntimeError(f'Build command failed ({p.returncode}); see {self.logdir}/{idx:04}.stderr and .stdout')
        except BaseException as e:row['error']=str(e);raise
        finally:row['ended']=time.time();(self.logdir/'commands.json').write_text(json.dumps(self.commands,indent=2)+'\n')
    def obj(self,component,src,flags,compiler=None,name=None):
        folder=self.build/'objects'/component;folder.mkdir(exist_ok=True)
        out=folder/(name or src.stem+'.o');self.run([compiler or self.cxx,*flags,'-c',src,'-o',out]);return out
    def dylib(self,name,objects,extra=()):
        out=self.build/'lib'/f'lib{name}.dylib'
        self.run([self.cxx,'-isysroot',self.sdk,'-dynamiclib','-Wl,-undefined,error',f'-Wl,-install_name,@rpath/{out.name}','-Wl,-rpath,@loader_path',*objects,*extra,'-o',out]);return out
    def executable(self,name,objects):
        out=self.build/'bin'/name;self.run([self.cxx,'-isysroot',self.sdk,*objects,'-o',out]);return out
    def source_paths(self,c):
        base=[ROOT/'tools/build.py',ROOT/'src/include']
        if c in ('stock','first','second'):return base+[ROOT/'src/backends'/c]
        if c=='accelerate':return base+[ROOT/'src/backends/stock/mp5_adapter.cpp']
        if c in ('libxsmm','kleidiai'):return base+[ROOT/'third_party'/c,ROOT/'src/backends'/f'{c}_adapter.cpp',ROOT/'src/backends/common.h',ROOT/'src/backends/preserved_call.S']
        if c=='openblas':return base+[ROOT/'third_party/openblas',ROOT/'src/backends/stock/mp5_adapter.cpp']
        return base+[ROOT/'src'/('harness' if c=='matrix' else c)]
    def component(self,c):
        sources=files_under(self.source_paths(c));old=self.manifest['components'].get(c,{})
        if old.get('status')=='PASS' and old.get('source_files')==sources and old.get('toolchain')==self.tools and all((self.build/p).is_file() and digest(self.build/p)==h for p,h in old['output_files'].items()):
            print('current:',c,flush=True);return
        print('building:',c,flush=True)
        record={'status':'BUILDING','source_files':sources,'toolchain':self.tools,'output_files':{},'commands':[]}
        self.manifest['components'][c]=record;self.save();start=len(self.commands)
        try:
            outputs=getattr(self,'build_'+c)()
            record.update(status='PASS',output_files={str(p.relative_to(self.build)):digest(p) for p in outputs})
        except BaseException as e:record.update(status='FAILED',error=str(e));raise
        finally:
            record['commands']=self.commands[start:];record['environment']={k:self.env[k] for k in ('TMPDIR','CLANG_MODULE_CACHE_PATH','PYTHONDONTWRITEBYTECODE','SDKROOT','LC_ALL')};self.save()
    def family(self,c):
        src=ROOT/'src/backends'/c
        objects=[self.obj(c,p,self.asm,self.cc) for p in sorted(src.glob('*.S'))]
        objects.append(self.obj(c,src/'smegemm.c',[*self.common,'-march=armv8-a+nosve','-fblocks','-ftree-vectorize','-I',src],self.cc))
        objects.append(self.obj(c,src/'mp5_adapter.cpp',[*self.cpp,*self.include,'-I',src,'-DMP5_STOCK_FAMILY=1',f'-DMP5_BACKEND_NAME="{c}"']))
        return [self.dylib(c,objects)]
    def build_stock(self):return self.family('stock')
    def build_first(self):return self.family('first')
    def build_second(self):return self.family('second')
    def build_accelerate(self):
        o=self.obj('accelerate',ROOT/'src/backends/stock/mp5_adapter.cpp',[*self.cpp,*self.include,'-DMP5_ACCELERATE=1','-DACCELERATE_NEW_LAPACK=1','-DMP5_BACKEND_NAME="accelerate"'])
        return [self.dylib('accelerate',[o],['-framework','Accelerate'])]
    def build_matrix(self):
        s=ROOT/'src/harness';strict=[*self.cpp,*self.include,'-fno-fast-math','-ffp-contract=off','-Wno-deprecated-declarations']
        objects=[self.obj('matrix',s/n,strict) for n in ('worker.cpp','inputs.cpp')]
        objects.append(self.obj('matrix',s/'call_guard.S',self.asm,self.cc))
        worker=self.executable('matrix-worker',objects)
        ref=self.obj('matrix',s/'reference.cpp',strict)
        test=self.obj('matrix',s/'test_backend.cpp',strict)
        return [worker,self.dylib('matrix-reference',[ref]),self.dylib('harness-test',[test,objects[1]])]
    def worktree(self,c):
        p=self.build/'dependencies'/c/self.logdir.name
        p.parent.mkdir(parents=True,exist_ok=True)
        shutil.copytree(ROOT/'third_party'/c,p)
        return p
    def build_libxsmm(self):
        s=self.worktree('libxsmm')
        self.run(['/usr/bin/make','-j1','lib','STATIC=1','OMP=0','THREADS=0','JIT=1','MNK=0','CACHE=0','DBG=0','SYM=0','CODE_BUF_MAXSIZE=131072',f'ECFLAGS=-DLIBXSMM_ENABLE_DEREG -fPIC -isysroot {self.sdk} -ffile-prefix-map={ROOT}=. -ffile-prefix-map={self.build}=build',f'CC={self.cc}',f'CXX={self.cxx}','FC=',f'PYTHON={sys.executable} -B',f'SDKROOT={self.sdk}',f'ECXXFLAGS=-isysroot {self.sdk}',f'ELDFLAGS=-isysroot {self.sdk}',f'BUILD_TMPDIR={self.build}/tmp','WERROR=0'],cwd=s)
        o=self.obj('libxsmm',ROOT/'src/backends/libxsmm_adapter.cpp',[*self.cpp,*self.include,'-I',s/'include'])
        guard=self.obj('libxsmm',ROOT/'src/backends/preserved_call.S',self.asm,self.cc)
        return [self.dylib('libxsmm',[o,guard,s/'lib/libxsmm.a'])]
    def build_kleidiai(self):
        s=ROOT/'third_party/kleidiai';objects=[]
        for i,n in enumerate(json.loads((s/'sources.json').read_text())):
            objects.append(self.obj('kleidiai',s/n,[*self.common,'-fno-tree-vectorize','-fno-tree-slp-vectorize','-march=armv8.2-a+sve+sve2','-I',s],self.cc,f'kernel-{i}.o'))
        objects.append(self.obj('kleidiai',ROOT/'src/backends/kleidiai_adapter.cpp',[*self.cpp,*self.include,'-I',s]))
        return [self.dylib('kleidiai',objects)]
    def build_openblas(self):
        s=self.worktree('openblas')
        flags=['TARGET=VORTEXM4','DYNAMIC_ARCH=0','USE_THREAD=0','NUM_THREADS=1','USE_OPENMP=0','NOFORTRAN=1','NO_LAPACK=1','BUILD_SINGLE=1','BUILD_DOUBLE=1','BUILD_COMPLEX=0','BUILD_COMPLEX16=0','INTERFACE64=0','BINARY=64',f'CC={self.cc}',f'HOSTCC={self.cc}',f'CFLAGS=-O2 -isysroot {self.sdk} -ffile-prefix-map={ROOT}=. -ffile-prefix-map={self.build}=build',f'LDFLAGS=-isysroot {self.sdk}','NO_SHARED=0','MAKE_NB_JOBS=1']
        self.run(['/usr/bin/make','-j1','shared',*flags],cwd=s)
        libs=list(s.glob('libopenblas*.dylib'));actual=[p for p in libs if not p.is_symlink()]
        if len(actual)!=1:raise RuntimeError('Expected one OpenBLAS shared library, found '+str(actual))
        out=self.build/'lib/libopenblas-native.dylib';shutil.copy2(actual[0],out)
        self.run(['install_name_tool','-id','@rpath/libopenblas-native.dylib',out])
        o=self.obj('openblas',ROOT/'src/backends/stock/mp5_adapter.cpp',[*self.cpp,*self.include,'-I',s,'-DMP5_OPENBLAS=1','-DMP5_BACKEND_NAME="openblas"'])
        adapter=self.dylib('openblas',[o,out]);return [adapter,out]
    def native_worker(self,c,srcname,outname):
        s=ROOT/'src'/c;objs=[self.obj(c,s/srcname,[*self.cpp,'-fno-fast-math','-ffp-contract=off'],name=Path(srcname).stem+'.o'),self.obj(c,s/'kernels.S',self.asm,self.cc)]
        return self.executable(outname,objs)
    def build_compute(self):return [self.native_worker('compute','worker.cpp','compute-worker')]
    def build_transfer(self):return [self.native_worker('transfer','worker.cpp','transfer-worker'),self.native_worker('transfer','qualification.cpp','transfer-qualification')]
    def build_ablation(self):
        s=ROOT/'src/ablation';out=[]
        for variant in ('blocked','grouped','online'):
            c='ablation_'+variant;offline=int(variant!='online');defines=[f'-DMP5_ABLATION_OFFLINE_PACK={offline}'];common=s/'common'
            objs=[self.obj(c,common/'adapter.cpp',[*self.cpp,*self.include,f'-DMP5_ABLATION_BACKEND_NAME="{c}"']),self.obj(c,common/'driver.cpp',[*self.cpp,*defines]),self.obj(c,common/'compat.S',[*self.asm,*defines],self.cc)]
            for p in sorted((s/variant).glob('*.S')):objs.append(self.obj(c,p,self.asm,self.cc))
            if offline:objs.append(self.obj(c,common/'packa.S',[*self.asm,f'-DMP5_ABLATION_GROUP4={int(variant=="grouped")}'],self.cc))
            out.append(self.dylib(c,objs))
        return out

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--build-dir',default='build');p.add_argument('component',choices=('all',*COMPONENTS),nargs='?',default='all');a=p.parse_args()
    build=Path(a.build_dir).expanduser().resolve()
    if build in (ROOT,ROOT.parent,Path('/')) or (build/'tools/build.py').exists():raise RuntimeError('Unsafe build directory; choose a dedicated generated-files directory.')
    build.mkdir(parents=True,exist_ok=True)
    with machine_lock(), (build/'.build.lock').open('w') as lock:
        try:fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
        except BlockingIOError:raise RuntimeError('Another build owns '+str(build))
        builder=Builder(build)
        for c in COMPONENTS if a.component=='all' else (a.component,):builder.component(c)
    print('Build complete:',build)
if __name__=='__main__':
    try:main()
    except Exception as e:print('ERROR:',e,file=sys.stderr);sys.exit(1)
