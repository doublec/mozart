#!/usr/bin/perl -w
#
# Authors:
#   Andreas Simon (2000)
#
# Copyright:
#   Andreas Simon (2000)
#
# Last change:
#   $Date$ by $Author$
#   $Revision$
#
# This file is part of Mozart, an implementation
# of Oz 3:
#   http://www.mozart-oz.org
#
# See the file "LICENSE" or
#   http://www.mozart-oz.org/LICENSE.html
# for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL
# WARRANTIES.
#

use Getopt::Long;
use POSIX qw(strftime);

$C_FUN_PREFIX = "oz"; # Prefix for OZ_BI_define'd functions

{
    my $time   = strftime "%a %b %e %H:%M:%S UTC %Y", gmtime;
    $C_WARNING = <<EOF;
/*
   THIS FILE IS AUTOMATICALLY GENERATED. DARE NOT TO EDIT OR BE FOREVER DAMNED!

                          $time
 */

EOF
}

# shortcuts
sub REF($) {
    return('OZ_declareForeignType ($i, $name, ' . $_[0] . ');');
}

@unsupported_functions =                     # mostly multiple return values
    ("gdk_input_window_get_pointer",
     "gdk_list_visuals",
     "gdk_query_depths",
     "gdk_query_visual_types",
     "gdk_selection_property_get",
     "gdk_text_property_to_text_list",
     "gdk_string_to_compound_text",
     "gdk_property_get",
     "gtk_args_collect",
     "gtk_arg_get_info",
     "gtk_args_query",
     "gtk_binding_entry_add_signal",
     "gtk_container_add_with_args",
     "gtk_container_child_args_collect",
     "gtk_container_child_set",
     "gtk_container_query_child_args",
     "gtk_container_child_set",
     "gtk_theme_engine_get",                  # bug in GTK 1.2.x, is fixed in CVS
     "gtk_theme_engine_ref",                  # bug in GTK 1.2.x, is fixed in CVS
     "gtk_theme_engine_unref",                # bug in GTK 1.2.x, is fixed in CVS
     "gtk_themes_init",                       # ???
     "gtk_object_class_user_signal_new",      # '...' argument
     "gtk_object_query_args",
     "gtk_object_args_collect",
     "gtk_object_arg_get_info",
     "gtk_object_new",
     "gtk_object_get",
     "gtk_object_set",
     "gtk_signal_connect",                    # handmade
     "gtk_signal_new",                        # handmade
     "gtk_signal_emit",                       # handmade
     "gtk_signal_emit_by_name",               # handmade
     "gtk_widget_new",
     "gtk_widget_set",
     # functions internal to GTK
     "gdk_draw_bitmap",
     "gtk_accel_group_active",
     "gtk_accel_group_attach",
     "gtk_accel_group_detach",
     "gtk_accel_group_get_entry",
     "gtk_accel_group_lock_entry",
     "gtk_accel_group_unlock_entry",
     "gtk_accel_group_add",
     "gtk_accel_group_remove",
     "gtk_accel_group_handle_add",
     "gtk_accel_group_handle_remove",
     "gtk_accel_group_create_add",
     "gtk_accel_group_create_remove",
     "gtk_accel_group_marshal_add",
     "gtk_accel_group_marshal_remove",
     "gtk_accel_groups_from_object",
     "gtk_accel_groups_entries_from_object",
     "gtk_themes_exit");
     
%argument_translation =
    ('void'               => ' ',
     'char'               => 'OZ_declareInt ($i, $name);',
     'unsigned char'      => 'OZ_declareInt ($i, $name);',
     'int'                => 'OZ_declareInt ($i, $name);',
     'double'             => 'OZ_declareFloat ($i, $name);',
     'int *'              => 'OZ_declareArray ($i, $name, int *);',
     '...'                => '... is not supported', # TODO: variable argument list
     'va_list'            => '/* TODO: va_list */',

     # GLib basic types
     'gboolean'           => 'GOZ_DECLARE_GBOOLEAN ($i, $name);',
     'gboolean *'         => 'OZ_declareArray ($i, $name, gboolean *);',

     'gpointer'           => 'GOZ_DECLARE_GPOINTER ($i, $name);',
     'gpointer *'         => 'OZ_declareForeignArray ($i, $name, gpointer *);',
     'gconstpointer'      => 'GOZ_DECLARE_GCONSTPOINTER ($i, $name);',

     'gchar'              => 'GOZ_DECLARE_GCHAR ($i, $name);',
     'gchar *'            => 'OZ_declareString ($i, $name);',
     'const gchar *'      => 'OZ_declareString ($i,$name);',
     'gchar **'           => 'gchar** $name;', #out
     'gchar ***'          => 'OZ_declareStringArray ($i, $name);',
     'guchar *'           => 'OZ_declareForeignArray ($i, $name, guchar *);',
     'guchar **'          => 'OZ_declareForeignArray ($i, $name, guchar **);',

     'gint'               => 'GOZ_DECLARE_GINT ($i, $name);',
     'gint *'             => 'GOZ_DECLARE_GINT ($i, $name_);' . "\n    " . 'gint * $name = &$name_;',
     'gint **'            => 'GOZ_DECLARE_GINT ($i, $name);', # TODO
     'guint'              => 'GOZ_DECLARE_GUINT ($i, $name);',
     'guint *'            => 'GOZ_DECLARE_GUINT ($i, $name_);' . "\n    " . 'guint * $name = &$name_;',
     'gshort'             => 'GOZ_DECLARE_GSHORT ($I, $name);',
     'gushort'            => 'GOZ_DECLARE_GUSHORT ($i, $name);',
     'glong'              => 'GOZ_DECLARE_GLONG ($i, $name);',
     'gulong'             => 'GOZ_DECLARE_GULONG ($i, $name);',
     'gulong *'           => 'OZ_declareArray ($i, $name, gulong *);',

     'gint8'              => 'GOZ_DECLARE_GINT8 ($i, $name);',
     'guint8'             => 'GOZ_DECLARE_GUINT8 ($i, $name);',
     'gint16'             => 'GOZ_DECLARE_GINT16 ($i, $name);',
     'guint16'            => 'GOZ_DECLARE_GUINT16 ($i, $name);',
     'gint32'             => 'GOZ_DECLARE_GINT32 ($i, $name);',
     'guint32'            => 'GOZ_DECLARE_GUINT32 ($i, $name);',
     'guint32 **'         => 'guint32 ** $name;', # TODO: This is in fact output
     'gint64'             => 'GOZ_DECLARE_GINT64 ($i, $name);',
     'guint64'            => 'GOZ_DECLARE_GUINT64 ($i, $name);',

     'gfloat'             => 'GOZ_DECLARE_GFLOAT ($i, $name);',
     'gdouble'            => 'GOZ_DECLARE_GDOUBLE ($i, $name);',

     # GLib compount types
     'GHashTable *'       => 'GOZ_DECLARE_GHASHTABLE ($i, $name);',
     'GQuark'             => 'GOZ_DECLARE_GQUARK ($i, $name);',
     'GList *'            => 'GOZ_DECLARE_GLIST ($i, $name);',
     'GSList *'           => 'GOZ_DECLARE_GSLIST ($i, $name);',
     'GSList **'          => 'GOZ_DECLARE_GSLIST2 ($i, $name);',
     'GScanner *'         => 'GOZ_DECLARE_GSCANNER ($i, $name);',

     # gdk
     'GDestroyNotify'     => 'GOZ_DECLARE_GDESTROYNOTIFY ($i, $name);',
     'GdkAtom'            => 'GOZ_DECLARE_GDKATOM ($i, $name);',
     'const GdkColor *'   => 'OZ_declareForeignType ($i, $name, GdkColor *);',
     'GdkDestroyNotify'   => 'GOZ_DECLARE_GDKDESTROYNOTIFY ($i, $name);',
     'GdkEvent *'         => 'OZ_declareGdkEvent ($i, $name);',
     'GdkEventFunc'       => 'GOZ_DECLARE_GDKEVENTFUNC ($i, $name);',
     'const GdkFont *'    => 'OZ_declareForeignType ($i, $name, GdkFont *);',
     'GdkFilterFunc'      => 'GOZ_DECLARE_GDKFILTERFUNC ($i, $name);',
     'GdkInputFunction'   => 'GOZ_DECLARE_GDKINPUTFUNC ($i, $name);',
     'GdkVisualType **'   => 'OZ_declareForeignArray ($i, $name, GdkVisualType **);', # helper
     'GdkWChar'           => 'GOZ_DECLARE_GCHAR ($i, $name);',
     'GdkWindow **'       => 'OZ_declareForeignArray ($i, $name, GdkWindow **);', # helper

     # gtk
     'GtkArgInfo **'      => 'GOZ_DECLARE_GTKARGINFO2 ($i, $name)',
     'GtkCallback'        => 'GOZ_DECLARE_GTKCALLBACK ($i, $name);',
     'GtkCallbackMarshal' => 'GOZ_DECLARE_GTKCALLBACKMARSHAL ($i, $name);',
     'GtkDestroyNotify'   => 'GOZ_DECLARE_GTKDESTROYNOTIFY ($i, $name);',
     'GtkEmissionHook'    => 'GOZ_DECLARE_GTKEMISSIONHOOK ($i, $name)',
     'GtkFunction'        => 'GOZ_DECLARE_GTKFUNCTION ($i, $name);',
     'GtkImageLoader'     => 'GOZ_DECLARE_GTKIMAGELOADER ($i, $name);',
     'GtkSignalDestroy'   => 'GOZ_DECLARE_GTKSIGNALDESTROY ($i, $name);',
     'GtkSignalMarshal'   => 'GOZ_DECLARE_GTKSIGNALMARSHAL ($i, $name);',
     'GtkSignalMarshaller' => 'GOZ_DECLARE_GTKSIGNALMARSHALLER ($i, $name);',
     'GtkType'            => 'GOZ_DECLARE_GTKTYPE ($i, $name);',
     'GtkType *'          => 'GOZ_DECLARE_GTKTYPE1 ($i, $name);', # Funky helper
     );

%result_translation =
    ('void'               => 'return OZ_ENTAILED;',
     'int'                => 'OZ_RETURN_INT (out_);',

     # GLib 
     'gint'               => 'OZ_RETURN_INT (out_);',
     'guchar'             => 'OZ_RETURN_INT (out_);',
     'guint'              => 'OZ_RETURN (OZ_unsignedInt (out_));',
     'guint32'            => 'OZ_RETURN (OZ_unsignedInt (out_));',
     'gulong'             => 'OZ_RETURN_INT (out_);',
     'gfloat'             => 'OZ_RETURN (OZ_float (out_));',
     'gdouble'            => 'OZ_RETURN (OZ_float (out_));',
     'gboolean'           => 'OZ_RETURN_BOOL (out_);',
     'gchar *'            => 'OZ_RETURN_STRING ((char *) out_);',
     'gchar **'           => 'GOZ_RETURN_GCHAR2 (out_);',
     'gpointer'           => 'OZ_RETURN (OZ_makeForeignPointer (out_));',
     'GNode *'            => 'GOZ_RETURN_GNODE (out_);',
     'GList *'            => 'OZ_RETURN (GOZ_GLIST_TO_OZTERM (out_));',
     'GSList *'           => 'OZ_RETURN (GOZ_GSLIST_TO_OZTERM (out_));',

     # Gdk
     'GdkAtom'            => 'OZ_RETURN_INT (out_);',
     'GdkEvent *'         => 'OZ_RETURN (OZ_gdkEvent (out_));',
     'GdkWindow *'        => 'OZ_RETURN (OZ_makeForeignPointer (out_));',

     # Gtk
     'GtkArg *'           => 'OZ_RETURN (OZ_makeForeignPointer (out_));', # not good
     'GtkArgInfo *'       => 'OZ_RETURN (OZ_argInfo (out_));', # helper
     'GtkReliefStyle'     => 'OZ_RETURN_INT ((int) out_);',
     'GtkType'            => 'OZ_RETURN (OZ_unsignedInt ((unsigned int) out_));',
     );


###############################################################################
#
#                           c_name_to_oz_feature
#
# Transforms a C name as c_name_to_oz_variable does, but the first character
# is forced to be lower case.
#
###############################################################################
sub c_name_to_oz_feature($) {
    my ($name) = @_;
    $name = c_name_to_oz_variable($name);
    $name = lcfirst($name);
    return $name;
}

###############################################################################
#
#                            c_name_to_oz_variable
#
# Transforms a C name to an Oz variable name, while fullfill Oz naming
# conventions. 'ozgtk_box_set_child_packing' gets 'BoxSetChildPacking'
# Is is asumed that all C names gbegin with 'ozgtk_'.
#
###############################################################################
sub c_name_to_oz_variable($) {
    my ($name) = @_;
    my @substrings;
    my $string;

    $name =~ s/^ozg[dt]k_//s;
    @substrings = split /_/, $name;
    foreach $string (@substrings) {
	$string = ucfirst $string;
    }
    $name = join '', @substrings;

    return $name;
}

###############################################################################
#
#                            init_enumerations
#
# Get information on enumerations 
#
###############################################################################

sub init_enumerations() {
    my $oldslash = $/;
    $/ = ";";

    foreach $header (@input) {
	open(INPUT, $header) || warn "Could not open file '$header'. $!\n";
	while (<INPUT>) {
	    s/^\s*\n$//mg;
	    s/\/\*[^\/]*\*\///g; # remove C comments
	    if (/(typedef(\s)+enum(\s)*\{)((\s*[^\n]*\n)+)\}([ \t\r\f]*)(\w*)/) { # enums
		$Enumerations{$7} = 1;
	    }
	}
	close INPUT;
    }

    $/ = $oldslash;
}

###############################################################################
#
#                            get_interface_data
#
# Parses C files for OZ_BI_define'd functions and generates an array containing
# the interface data as described below.
# For each interface procedure found the array contains an reference to another
# array which contains all interface information for this procedure.
# The first field of the array is the C name of the procedure.
# The second field is the Oz name of the procedure.
# The third field is the in arity of the procedure.
# The forth field is the out arity. 
#
###############################################################################
sub get_interface_data(@) {
    my @c_files = @_;
    my $file;
    my @interface_data;

    $/ = "\n";

    foreach $file (@c_files) {
	open(INPUT, $file) || warn "Could not open file '$file'. $!\n";
	while (<INPUT>) {
	    if(m/OZ_BI_define\s*\((\w+)\,\s*(\d+)\s*\,\s*(\d+)\s*\)/) {
		my $c_name = $1;
		my $oz_name = c_name_to_oz_feature($c_name);
		my $in_arity = $2;
		my $out_arity = $3;
		my @list = ($c_name, $oz_name, $in_arity, $out_arity);
		@interface_data = (@interface_data, \@list);
	    }
	}
	close INPUT;
    }
    return @interface_data;
}

###############################################################################
#
#                              gen_oz_interface
#
# searches C files for OZ_BI_defined functions and generates prototypes and the
# Oz/C procedure interface and writes them in a file
# input: an array of file names
#
###############################################################################
sub gen_oz_interface(@) {
    my @interfaces = get_interface_data @_;
    my $interface;

    print $C_WARNING;
    print <<EOF;
#include <gtk/gtk.h>
#include <mozart.h>

EOF

    # generate prototypes for the interface functions
    foreach $interface (@interfaces) {
	my $c_name = $$interface[0];
	print "OZ_BI_proto($c_name);\n";
    }

    # oz_init_module
    print <<EOF;

OZ_C_proc_interface *
oz_init_module()
{
    static OZ_C_proc_interface interface[] = {
EOF
    
    # generate interface definitions
    foreach $interface (@interfaces) {
	my $c_name    = $$interface[0];
	my $oz_name   = $$interface[1];
	my $in_arity  = $$interface[2];
	my $out_arity = $$interface[3];
	print "        {\"$oz_name\", $in_arity, $out_arity, $c_name},\n";
    }

    print <<EOF;
        {0, 0, 0, 0}
    };
    gtk_init (0, 0);
    return interface;
}
EOF
	
}

###############################################################################
#
#                        get_function_declarations
#
# Parses C header files for function declarations and returns them in an array.
# For every function declaration the array contains an reference to an array of
# the following build.
# The first field contains the procedure name.
# The second field contains the output type.
# The third field is a reference to an array containing the procedure arguments.
# This array of arguments contains "void" or alternately types and variable
# names.
#
###############################################################################
sub get_function_declarations(@) {
    my @files = @_;
    my $file;
    my @declarations;

    $/ = ";"; # field separator

    foreach $file (@files) {
	open(FILE, $file) || die "Could not open file $file. $!";
      GET_NEXT_FUNC_DEC:
	while (<FILE>) {
	    # search for function declarations
	    s/\/\*[^\/]*\*\///g;    # remove c comments
	    s/^\s*\n$//mg;          # remove empty lines
	    next if /^\s*typedef/m; # no typedefs
#	    next unless /^([\w\*]+)\s+(gdk\w+|gtk\w+)\s*\(([^\)]*)\)/m; # only GTK/GDK prototypes
	    next unless /^([\w\*]+\s+|\w+\s+\**\s*)(gdk\w+|gtk\w+)\s*\(([^\)]*)\)/m; # only GTK/GDK prototypes
	    my $result_type = $1;
	    my $function_name = $2;
	    my @args = split /\,/, $3; # separate function arguments
	    my @argument_list;

	    $result_type =~ s/(\S+)\s+$/$1/s ;
	    $result_type =~ s/(\w+)\s+\*/$1 */;
	    $result_type =~ s/^\s+(\w)/$1/s;

	    # Maybe the function is declared previously, then do nothing
	    foreach my $dec (@declarations) {
		if ($function_name eq $$dec[0]) {
		    next GET_NEXT_FUNC_DEC;
		}
	    }
	    
	    # Process function arguments
	    foreach my $arg (@args) {
		$arg =~ s/^\s+//g; # delete trailing spaces
		if ($arg =~ /^([\w\*]+(\s+\w+)?\s+\**\s*)(\w+)$/) { # form '<type> <variable name>'
		    my $type = $1;
		    my $var  = $3;
		    $type =~ s/\s+\*/ \*/g; # nicer formatting
		    $type =~ s/\s+$//g; # eliminate spaces
		    @argument_list = (@argument_list, $type, $var);
		} elsif ($arg =~ /^const gchar\s*\*\s*(\w+)/) {     # const gchar *
		    @argument_list = (@argument_list, "const gchar *", $1);
		} elsif ($arg =~ /^(\w+)\s+(\w+)\[\]/) {            # array types
		    @argument_list = (@argument_list, "$1 *", $2);
		} elsif ($arg =~ /^(\w+)\s*\*\s*(\w+)\[\]/) {       # array of pointers
		    @argument_list = (@argument_list, "$1 **", $2);
		} elsif ($arg =~ /^void/) {
		    @argument_list = ("void", ""); # void type has no variable name
		} elsif ($arg =~ /^\.\.\.$/) {
		    @argument_list = (@argument_list, "...", "");
		} else {
		    die "parse error in subroutine get_function_declarations "
			. "while parsing '$arg' in file $file";
		}
	    }
	    # Add new declaration to the list
	    $result_type =~ s/(\w)\*/$1 \*/g;
	    my @dec  = ($function_name, $result_type, \@argument_list);
	    @declarations = (@declarations, \@dec);
	}
	close FILE;
    }
    return @declarations;
}

###############################################################################
#
#                        get_macro_definitions
#
###############################################################################

#  sub get_macro_definitions(@) {
#      my @files = @_;
#      my $file;
#      my @definitions;

#      foreach $file (@files) {
#  	open(FILE, $file) || die "Could not open file $file. $!";
#  	while (<FILE>) {
#  	    # search for macro definitions
#  	    s/\/\*[^\/]*\*\///g; # remove c comments
#  	    next unless /^\#define\s+(G[TD]K\w+)\(([\w,]+)\)/m; # only GTK/GDK prototypes
#  	    print "debug: recognize a macro '$1'\n";
#  	    my $macro_name    = $1;
#  	    my @argument_list = split /\,/, $2;
#  	    my @macro         = ($macro_name, @argument_list);
#  	    @definitions = (@definitions, \@macro);
#  	}
#  	close FILE;
#      }
#      return @definitions;
#  }

###############################################################################
#
#                           gen_oz_bi_definition
#
# Generates an procedure definition via the Oz/C++ interface macros.
# It takes an reference to an array as it is returned by
# get_function_declarations (but only one reference, no array of references).
#
###############################################################################
sub gen_oz_bi_definition($) {
    my ($fun) = @_;
    my $fun_name  = $$fun[0];
    my $fun_out   = $$fun[1];
    my $fun_in    = $$fun[2];
    my $out_arity = ($fun_out =~ m/void/) ? 0 : 1;
    my $in_arity  = ($$fun_in[0] =~ m/^void$/) ? 0 : @$fun_in / 2; # array contains types and names
    my $def;

    # no auto-generated code for functions which return multiple values
    foreach my $i (@unsupported_functions) {
	if ($fun_name eq $i) { return "" };
    }

    $def  = "OZ_BI_define ($C_FUN_PREFIX$fun_name, $in_arity, $out_arity) {\n";

    # declare output value
    if ($out_arity == 1) {
	$def .= "    $fun_out out_;\n";
    }

    #
    # define input parameters
    #
    for (my $i = 0; $i < $in_arity; $i++) {
	my $type = $$fun_in[2 * $i];
 	my $var  = $$fun_in[2 * $i + 1];
	my $code;

	# Translation:
	# For finding the right translation we first check
	# if there's a entry in the translation hash and if
	# not, we take a default translation.

	# we don't need the const keyword
	$type =~ s/const\s+//g;
	
	if($argument_translation{$type}) {
	    $code = $argument_translation{$type};
	}
	# Default for enumerations
	elsif($Enumerations{$type}) {
	    $code = 'OZ_declareInt ($i, $name_);' . "\n    "
		. $type . ' $name = (' . $type . ') $name_;';
	}
	# Default for GTK objects
	elsif($type =~ /^(const *)?(Gtk\w+\s*\*)$/) {
	    $code = 'OZ_declareForeignType ($i, $name, ' . $2 . ');';
	}
	# Default for arrays of GTK objects
	elsif($type =~ /^(Gtk\w+ \*\*)$/) {
	    $code = 'OZ_declareForeignArray ($i, $name, ' . $1 . ');';
	}
	# Default for GDK types
	elsif($type =~ /^(const )?(Gdk\w+ \*)$/) {
	    $code = 'OZ_declareForeignType ($i, $name, ' . $2 . ');';
	}
	# Default for arrays GDK types
	elsif($type =~ /^(Gdk\w+ \*\*)$/) {
	    $code = 'OZ_declareForeignArray ($i, $name, ' . $1 . ');';
	}
	# Default basic pointer types
	elsif($type =~ /^\w+ \*/) {
	    $code = 'OZ_declareForeignArray ($i, $name, ' .  $type . ');';
	}
	elsif($type =~ /^\w+ \*\*/) {
	    $code = 'OZ_declareForeignArray ($i, $name, ' . $type . ');';
	}
	elsif($type =~ /^\w+ \*\*\*/) {
	    $code = 'OZ_declareForeignArray ($i, $name, ' . $type . ');';
	}
	# Default function types
	elsif($type =~ /^\w+Func$/) {
	    $code = 'GOZ_DECLARE_FUNCTION ($i, $name, ' . $type . ');';
	}
	else { die "Parse error while parsing '$type' in function '$fun_name'"; }
	
	$code =~ s/\$i/$i/g;
	$code =~ s/\$name/$var/g;
	$def .= '    ' . $code . "\n";
    } 

    # invoke the GTK function
    $def .= '    ';
    $def .= "out_ = " unless $out_arity == 0;
    $def .= "$fun_name (";

    # fillin the arguments
    for (my $k = 0; $k < $in_arity; $k++) {
	# type of argument is $$fun_in[2 * $k]
	$def .= "$$fun_in[2 * $k + 1]"; # variable name
	$def .= ', ' if ($k < $in_arity - 1);
    }
    $def .= ");\n";

    # return results
    # TODO: support more than one result
    # First we check the translation table. If there's no entry,
    # default behaviors of various type classes match.

    $def .= '    ';

    if($result_translation{$fun_out}) {
	$def .= $result_translation{$fun_out};
    }
    elsif($fun_out =~ /(Gtk\w+ \*)/) {
	$def .= 'OZ_RETURN (OZ_makeForeignPointer (out_));';
    }
    elsif($fun_out =~ /(Gdk\w+ \*)/) {
	$def .= 'OZ_RETURN (OZ_makeForeignPointer (out_));';
    }
    elsif($Enumerations{$fun_out}) {
	$def .= 'OZ_RETURN_INT ((int) out_);',
    }
    else { die "Parse error while parsing '$fun_out'"; }

    $def .= "\n} OZ_BI_end\n";

    return $def;
}

###############################################################################
#
#                              gen_c_wrappers
#
# Takes a list of GTK headers and generates the appropriate C wrappers for them
#
###############################################################################
sub gen_c_wrappers(@) {
    my @fun_decs = get_function_declarations(@_);
    #my @macro_defitions = get_macro_definitions(@_);
    my $fun;
    my $macro;

    print  $C_WARNING;
    print  <<EOF;
#include <gtk/gtk.h>
#include <mozart.h>
#include <gozsupport.h>

EOF

    # deal with functions
    foreach $fun (@fun_decs) {
	print gen_oz_bi_definition($fun) . "\n";
    }

    # deal with macros
#    foreach $macro (@macro_defitions) {
#	print "debug: $$macro \n";
#    }
}

###############################################################################
#
#                             gen_oz_templates
#
###############################################################################
sub gen_oz_templates(@) {
    my @fun_decs = get_function_declarations(@_);


}

###############################################################################
#
#                                   usage
#
###############################################################################
sub usage() {
    print <<EOF;
usage: $0 [OPTION] [INPUT FILE] ...

Generate glue code for the GTK+ binding for Oz

  --c-wrappers              generate the C glue code
  --oz-templates            generate templates for the Oz classes
  --interface               generate the interface definition for
                            Oz\' C/C++ interface
EOF

    exit 0;
}

###############################################################################
#
#                                   main
#
###############################################################################

# parse arguments
$opt_cwrappers = $opt_oztemplates = $opt_interface = 0;
&GetOptions("c-wrappers"     =>    \$opt_cwrappers,
	    "oz-templates"   =>    \$opt_oztemplates,
	    "interface"      =>    \$opt_interface,);

@input = @ARGV;

usage() unless @input != 0;
usage() unless $opt_cwrappers | $opt_oztemplates | $opt_interface; 

init_enumerations()       if $opt_cwrappers;
gen_c_wrappers   (@input) if $opt_cwrappers;
gen_oz_templates (@input) if $opt_oztemplates;
gen_oz_interface (@input) if $opt_interface;
