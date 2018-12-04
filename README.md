# FPS

高性能快速数据收发，基于dpdk/odp/ofp框架；


centos7.3

02:00.0 Ethernet controller: Intel Corporation 82599ES 10-Gigabit SFI/SFP+ Network Connection (rev 01)
02:00.1 Ethernet controller: Intel Corporation 82599ES 10-Gigabit SFI/SFP+ Network Connection (rev 01)

[root@localhost ofpk]# cat /proc/version 
Linux version 3.10.0-514.el7.x86_64 (builder@kbuilder.dev.centos.org) (gcc version 4.8.5 20150623 (Red Hat 4.8.5-11) (GCC) ) #1 SMP Tue Nov 22 16:42:41 UTC 2016


[root@localhost ofpk]# lscpu
Architecture:          x86_64
CPU op-mode(s):        32-bit, 64-bit
Byte Order:            Little Endian
CPU(s):                16
On-line CPU(s) list:   0-15
Thread(s) per core:    1
Core(s) per socket:    16
Socket(s):             1
NUMA node(s):          1
Vendor ID:             GenuineIntel
CPU family:            6
Model:                 95
Model name:            Intel(R) Atom(TM) CPU C3958 @ 2.00GHz
Stepping:              1
CPU MHz:               800.000
CPU max MHz:           2000.0000
CPU min MHz:           800.0000
BogoMIPS:              4000.01
Virtualization:        VT-x
L1d cache:             24K
L1i cache:             32K
L2 cache:              2048K
NUMA node0 CPU(s):     0-15

性能统计UDP报文收发能力
CPU[4]: TX 1.826654 Gbps (max 1.831570), 0.178384 Mpps (max 0.178864)
CPU[5]: TX 1.865535 Gbps (max 1.877975), 0.182181 Mpps (max 0.183396)
CPU[6]: TX 1.841618 Gbps (max 1.848659), 0.179845 Mpps (max 0.180533)
CPU[7]: TX 1.850054 Gbps (max 1.860394), 0.180669 Mpps (max 0.181679)
CPU[8]: TX 1.830057 Gbps (max 1.831605), 0.178716 Mpps (max 0.178867)
Total TX 99659.48 GBytes, number of send() calls 77858972645, avg bytes per call 1280.00

CPU[4]: RX 0.998331 Gbps (max 1.158698), 0.121985 Mpps (max 0.141580)
CPU[5]: RX 0.998676 Gbps (max 1.158937), 0.122027 Mpps (max 0.141610)
CPU[6]: RX 0.999084 Gbps (max 1.158989), 0.122077 Mpps (max 0.141616)
CPU[7]: RX 0.998932 Gbps (max 1.159114), 0.122059 Mpps (max 0.141631)
CPU[8]: RX 0.998300 Gbps (max 1.000152), 0.121981 Mpps (max 0.122208)
Total RX 1687.56 GBytes, number of recv() calls 1649620597, avg bytes per call 1023.00
Total packets: 1649618135
Per size:
     0- 128: 216
   128- 512: 0
   512-1024: 0
  1024-1280: 1649617919
  1280- MTU: 0

