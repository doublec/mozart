%%%
%%% Authors:
%%%   Michael Mehl (mehl@dfki.de)
%%%
%%% Copyright:
%%%   Michael Mehl, 1998
%%%
%%% Last change:
%%%   $Date$ by $Author$
%%%   $Revision$
%%%
%%% This file is part of Mozart, an implementation
%%% of Oz 3
%%%    http://mozart.ps.uni-sb.de
%%%
%%% See the file "LICENSE" or
%%%    http://mozart.ps.uni-sb.de/LICENSE.html
%%% for information on usage and redistribution
%%% of this file, and for a DISCLAIMER OF ALL
%%% WARRANTIES.
%%%

functor

import
   FD
export
   Return

define
   Return = fd([
		simple(proc {$}
			  X Y S in
			  X::1#100000 Y::1#100000
			  S={Space.new
			     proc {$ _}
				X<:Y Y<:X
			     end}
			  {Wait {Space.ask S}}
		       end
		       keys:[bench fd propagator]
		       bench:1)
	       ])
end
