#step 1.
#volatile.
:- not a.
#endstep.

#step 1.
#volatile.
e(1).
#volatile.
:- not b.
#endstep.

#step 2.
#volatile.
:- not a.
#endstep.

#step 2.
#volatile.
:- not b.
#endstep.

#step 2.
#volatile.
:- not a.
#endstep.

#step 6.
#volatile.
:- not b.
#cumulative.
e(6).
#endstep.

#step 9.
e(9).
#endstep.

#stop.
