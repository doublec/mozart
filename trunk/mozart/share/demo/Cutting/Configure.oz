%%%
%%% Authors:
%%%   Christian Schulte <schulte@dfki.de>
%%%
%%% Copyright:
%%%   Christian Schulte, 1998
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

export
   normal: Helv
   bold:   HelvBold

   colors: Colors
   
prepare

   local
      HelvFamily        = '-*-helvetica-medium-r-normal--*-'
      HelvBoldFamily    = '-*-helvetica-bold-r-normal--*-'
      FontMatch         = '-*-*-*-*-*-*'
      
      FontSize          = 120
   in
      [HelvBold Helv] =
      {Map [HelvBoldFamily    # FontSize  # FontMatch
	    HelvFamily        # FontSize  # FontMatch]
       VirtualString.toAtom}
   end

   Colors = colors(glass:   steelblue1
		   cut:     firebrick
		   sketch:  c(202 225 255)
		   entry:   wheat
		   bad:     c(238 44 44)
		   okay:    c(255 127 0)
		   good:    c(180 238 180)
		   neutral: ivory
		   bg:      ivory)

   Delays = delays(cut:  400
		   wait: 1200)

end
