functor

import

   FD
   Search
   
export
   Return
define

   MiscTest =
   fun {$ N T}
      L = {StringToAtom {VirtualString.toString N}}
   in
      L(equal(T 1) keys: [fd])
   end
   

   Return=
   fd([misc2([
	      {MiscTest 1
	       fun {$} X Y Z D in X = 3 Z = 1 {FD.decl D}
		  thread {FD.sumC [1 2 3] [X Y Z] '=:' D} end
		  Y = 2 
		  cond D = 10 then 1 else 0 end
	       end}
	      
	      {MiscTest 2
	       fun {$} X Y Z D in X = 3 Z = 1 {FD.decl D}
		  thread {FD.sumC a(1 2 3) a(a: X b:Y z:Z) '=:' D} end
		  Y = 2 
		  cond D = 10 then 1 else 0 end
	       end}
     
	      {MiscTest 3
	       fun {$} X Y Z D in X = 3 Z = 1 {FD.decl D}
		  thread {FD.sumC a( c:3 a:1 b:2 ) a(a: X k:Y w:Z) '=:' D} end
		  Y = 2 
		  cond D = 10 then 1 else 0 end
	       end}
     
	      {MiscTest 4
	       fun {$} X Y Z D in {FD.decl D}
		  thread {FD.sumCN [1 2 3] [[X X] a(Y Y) b(x:Z y:Z)] '=:' D} end
		  Y = 2
		  Z = 4
		  X = 3 
		  cond D = 65 then 1 else 0 end
	       end}
     
	      {MiscTest 5
	       fun {$} X Y Z D in {FD.decl D}
		  thread {FD.sumCN [1 2 3] a([X X] a(Y Y) b(x:Z y:Z)) '=:' D} end
		  Y = 2
		  Z = 4
		  X = 3 
		  cond D = 65 then 1 else 0 end
	       end}
	      
	      {MiscTest 6
	       fun {$} X Y Z D in {FD.decl D}
		  thread {FD.sumCN [1 2 3] b(x:[X X] y:a(Y Y) z:b(x:Z y:Z)) '=:' D} end
		  Y = 2
		  Z = 4
		  X = 3 
		  cond D = 65 then 1 else 0 end
	       end}

	      {MiscTest 7
	       fun {$}
		  proc{P Sol}
		     Numer1 Denom1 Numer2 Denom2 Numer3 Denom3
		     B1 B2 B3
		  in
		     Sol = [Numer1 Denom1 Numer2 Denom2 Numer3 Denom3]
		     [Numer1 Denom1 Numer2 Denom2 Numer3 Denom3] ::: 1#2
		     [B1 B2 B3] ::: 0#1
		     thread
			((Numer1 * Denom2) <: (Numer2 * Denom1)) = B1
			((Numer1 * Denom3) =: (Numer3 * Denom1)) = B2
			((Numer2 * Denom3) =: (Numer3 * Denom2)) = B3
		     end
		     {FD.distinct [B2 B3]}
		     {FD.impl B1 B3 1}
		     {FD.impl {FD.nega B1} B2 1}
		     {FD.distribute naive Sol}
		  end
		  AllSols = {Search.base.all P}
	       in
		  cond {ForAll AllSols P} then 1 else 0 end
	       end}
	     ])
      ])
end

