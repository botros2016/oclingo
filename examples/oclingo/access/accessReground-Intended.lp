% This example provides a (fixed-size) sliding window implementation over aging
% logic programs, where recent external events are evaluated in own (volatile)
% programs. Take, e.g., streams/stream01-14E.str as (interesting) logic program
% stream, where comments in streams/stream01-14E.str indicate desired outcomes.

%%%%%%%%%%%%%
% CONSTANTS %
%%%%%%%%%%%%%

#const window=3.                 % external events in window for 3 steps (by default)
#const offset=2.                 % time points/terms may deviate up to 2 (by default)
#const denial=3.                 % account closed when 3 accesses denied (by default)

#iinit 1-offset.

%%%%%%%%
% BASE %
%%%%%%%%

#base.

user(bob;alice;claude). % some users
signal(denied;granted). % some signals

{ account(U,closed) : user(U) }.                      % guess whether accounts closed
  account(U,open)  :- user(U), not account(U,closed). % or open otherwise


%%%%%%%%%%%%
% VOLATILE %
%%%%%%%%%%%%

#volatile t.


% expect user accesses at positive time points up to "offset" time beyond step number
#external access(U,S,t+offset) : user(U) : signal(S).

% revoke user accesses before time point t-offset (atoms declared at step t-2*offest)
#forget t : t-2*offset-1.

% time points in current window
time(t,t-(window+offset)+1..t+offset) :- 0 < t.

% count consecutive denied user accesses up to a "denial" threshold
denied(t,U,1,  T)   :- access(U,denied,T), user(U), time(t,T).
denied(t,U,N+1,T+1) :- access(U,denied,T+1), denied(t,U,N,T), N < denial.

% account closed if "denial" many consecutive denied user accesses observed in window

:- denied(t,U,denial,T), not account(U,closed).

% account open if consecutive denied user accesses observed in window below threshold

:- account(U,closed), not denied(t,U,denial,T) : time(t,T).


%%%%%%%%%%%%%%%%%%%%%
% OUTPUT PROJECTION %
%%%%%%%%%%%%%%%%%%%%%

#hide.
#show access/3.  % display user accesses in window
#show account/2. % and the states of user accounts
% #show denied/4.  % and denied user access counters
