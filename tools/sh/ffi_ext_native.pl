#!/usr/bin/perl
# ffi_ext_native.pl <file.inc> <dir>... — DECLARE con array E parametri interi espliciti (la forma la
# definisce la funzione): le marca NATIVE con l'ABI esatta. Se il simbolo e' uno shim (__ffi) torna
# all'originale; se la funzione C porta le estensioni in-place (arita' = parametri + array) genera in
# coda al sorgente un trampolino <sym>__nat con la firma originale che passa INT64_MAX come estensione.
use strict; use warnings; use File::Find;
my ($inc, @dirs) = @ARGV; open(my $fh,'<',$inc) or die; my @lines = <$fh>; close $fh;
sub split_top { my ($s)=@_; my @o; my ($d,$cur)=(0,''); for my $ch (split //,$s) { $d++ if $ch eq '('; $d-- if $ch eq ')'; if ($ch eq ',' && $d==0) { push @o,$cur; $cur='' } else { $cur.=$ch } } push @o,$cur if length($cur)||@o; return @o; }
my @f; find(sub { push @f,$File::Find::name if /\.c$/ }, @dirs); my %src; for my $x (@f) { open(my $c,'<',$x) or next; local $/; $src{$x}=<$c>; close $c; }
my (%tramp, %file_of, @out, $nmark, @manual);
for my $l (@lines) {
  if ($l =~ /^(\s*(?:PUBLIC\s+|PRIVATE\s+)?DECLARE)\s+(FUNCTION|SUB)\s+(\w+)\s+LIB\s+("[^"]+")(?:\s+ALIAS\s+"([^"]+)")?(\s*\((.*)\)\s*(?:AS\s+\w+)?\s*)$/i) {
    my ($pre,$kind,$name,$lib,$alias,$post,$params)=($1,$2,$3,$4,$5,$6,$7); $post =~ s/\s+$//;
    my @ps = split_top($params); my @arr = grep { $ps[$_] =~ /^\s*(?:BYREF\s+|BYVAL\s+)?\w+\(\)\s+AS/i } 0..$#ps;
    my $ints = grep { $ps[$_] !~ /\(\)\s+AS/ && $ps[$_] =~ /\bAS\s+(LONG|INTEGER|QUAD|SHORT|BYTE|ULONG|UINTEGER)\b/i && $ps[$_] !~ /FUNCTION|SUB|BYREF/i } 0..$#ps;
    if (@arr && $ints) {
      my $sym = $alias // $name; (my $base = $sym) =~ s/__ffi$//;
      my $newalias = $base; my $done = 1;
      my $found;
      for my $x (sort keys %src) { if ($src{$x} =~ /(?:^|\n)((?:static\s+)?[A-Za-z_][\w\s\*]*?)\b\Q$base\E\s*\(([^)]*)\)\s*\{/) { $found = [$x,$1,$2]; last; } }
      if ($found) {
        my ($x,$rt,$cp) = @$found; my @cps = split_top($cp);
        if (@cps == @ps + @arr && $rt !~ /\bstatic\b/) {
          # estensioni in-place: trampolino con la firma originale
          my (@sig,@args); my %isext; for my $k (@arr) { $isext{$k + (grep { $_ < $k } @arr) + 1} = 1; }
          my $ok = 1;
          for my $k (0..$#cps) {
            my $q=$cps[$k]; $q =~ s/^\s+|\s+$//g; my ($nm)=$q =~ /(\w+)\s*(?:\[\s*\w*\s*\])?$/;
            if ($isext{$k}) { push @args,'INT64_MAX'; next }
            unless (defined $nm && ($q =~ /\s/ || $q =~ /\*\s*\w+$/)) { $ok=0; last }
            push @sig,$q; push @args,$nm;
          }
          if ($ok) {
            $rt =~ s/^\s+|\s+$//g; my $ret = $rt eq 'void' ? '' : 'return ';
            $tramp{$base} = sprintf("%s %s__nat(%s) { %s%s(%s); }\n",$rt,$base,join(', ',@sig),$ret,$base,join(', ',@args));
            $file_of{$base}=$x; $newalias = "${base}__nat";
          } else { push @manual,$base; $done = 0; }
        } elsif (@cps != @ps) { push @manual,"$base (C=".scalar(@cps)." DECLARE=".scalar(@ps).")"; $done = 0; }
      }
      if ($done) {
        my $al = (defined $alias || $newalias ne $name) ? " ALIAS \"$newalias\"" : "";
        $l = "$pre NATIVE $kind $name LIB $lib$al$post\n"; $nmark++;
      }
    }
  }
  push @out,$l;
}
my %by; push @{$by{$file_of{$_}}},$_ for keys %tramp;
for my $cf (sort keys %by) {
  my @todo = grep { $src{$cf} !~ /\b\Q$_\E__nat\s*\(/ } sort @{$by{$cf}}; next unless @todo;
  open(my $o,'>>',$cf) or die;
  print $o "\n#include <stdint.h>\n/* Trampolini DECLARE NATIVE (ffi_ext_native.pl): firma originale, estensioni = INT64_MAX. */\n";
  print $o $tramp{$_} for @todo; close $o;
}
open(my $w,'>',$inc) or die; print $w @out; close $w;
printf "marcate NATIVE %d, trampolini %d%s\n", $nmark//0, scalar(keys %tramp), @manual ? ", MANUALI: ".join(", ",@manual) : "";
