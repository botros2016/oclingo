h(a(1)).
{ h(b(X)), h(c(X)), not h(n(X)) } :- h(a(X)).
h(d(X)) :- h(b(X)), h(c(X)).

a(X,X,X) :- h(a(X)), c.
b      :- a(1,2,X).
b      :- h(c(1)).
c      :- b.
