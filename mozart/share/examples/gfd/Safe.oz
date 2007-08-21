%%%
%%% Authors:
%%%     Gustavo Gutierrez <ggutierrez@cic.puj.edu.co>
%%%     Alberto Delgado <adelgado@cic.puj.edu.co>
%%%     Alejandro Arbelaez <aarbelaez@puj.edu.co>
%%%
%%% Copyright:
%%%     Gustavo Gutierrez, 2006
%%%     Alberto Delgado, 2006
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

declare

proc{Safe C}
   C1 C2 C3 C4 C5 C6 C7 C8 C9
in
   C = [C1 C2 C3 C4 C5 C6 C7 C8 C9]
   C:::1#9
   {GFD.distinct  C GFD.cl.bnd}

   {GFD.linear2 [1 ~1 ~1] [C4 C6 C7] GFD.rt.'=:' 0 GFD.cl.val}
   local Tmp1 Tmp2 in
      Tmp1 = {GFD.decl}
      Tmp2 = {GFD.decl}
      {GFD.mult C1 C2 Tmp1}
      {GFD.mult Tmp1 C3 Tmp2}
      {GFD.linear2 [1 ~1 ~1] [Tmp2 C8 C9] GFD.rt.'=:' 0 GFD.cl.val}
   end
   {GFD.linear2 [1 1 1 ~1] [C2 C3 C6 C8] GFD.rt.'<:' 0 GFD.cl.val}
   {GFD.linear2 [1 ~1] [C9 C8] GFD.rt.'<:' 0 GFD.cl.val}
   for I in 1..9 do
      {GFD.rel {List.nth C I} GFD.rt.'\\=:' I GFD.cl.val}
   end
   {GFD.distribute ff C}
end

{Show {SearchAll Safe}}