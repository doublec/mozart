%%%
%%% Authors:
%%%     Alejandro Arbelaez <aarbelaez@puj.edu.co>
%%%
%%% Copyright:
%%%     Alejandro Arbelaez, 2006
%%%
%%% Last change:
%%%   $Date: 2006-10-19T01:44:35.108050Z $ by $Author: ggutierrez $
%%%   $Revision: 2 $
%%%
%%% This file is part of Mozart, an implementation
%%% of Oz 3
%%%    http://www.mozart-oz.org
%%%
%%% See the file "LICENSE" or
%%%    http://www.mozart-oz.org/LICENSE.html
%%% for information on usage and redistribution
%%% of this file, and for a DISCLAIMER OF ALL
%%% WARRANTIES.
%%%

%%% Adapted from a finite domain example in Mozart-Oz version 1.3.2 by 
%%% Gert Smolka, 1998.

declare

proc{Money Root}
    S E N D
    M O R Y
    RootVal
 in
   Root =  [S E N D M O R Y]
   
   RootVal = [1000 100 10 1 1000 100 10 1 ~10000 ~1000 ~100 ~10 ~1]
   Root:::0#9
   {GFD.sumC RootVal
		[S E N D M O
		 R E M O N E Y]
		 '=:' 0
		 }
   
   S \=: 0
   M \=: 0
  
   {GFD.distinctP  post(Root cl:GFD.cl.bnd)}
   {GFD.distributeBR generic(order:naive value:max) Root}
   %{Wait {GFD.distributeC Root}}
end


{Show {SearchOne Money}}



