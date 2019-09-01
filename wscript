## -*- Mode: python; py-indent-offset: 4; indent-tabs-mode: nil; coding: utf-8; -*-
def configure(cnf):
    # other parameters omitted for brevity
    cnf.check(lib=["kronosapi"])

def build(bld):
    #bld(use=["kronosapi"])
    obj = bld.create_ns3_program('simulation', ['csma', 'tap-bridge', 'mobility', 'wifi', 'internet'])
    obj.source = ['simulation.cc', 'start_lxcs.cc']
    obj.env.append_value("LINKFLAGS", ["-lkronosapi"])
    obj.env.append_value("LIB", ["kronosapi"])
    
