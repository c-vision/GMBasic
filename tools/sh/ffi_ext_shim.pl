#!/usr/bin/perl
# ffi_ext_shim.pl — per le funzioni native con chiamanti interni (non ofx_*): genera uno SHIM
# `<sym>__ffi` con la firma FFI (int64_t di estensione dopo ogni array) che chiama l'originale,
# e punta l'ALIAS della DECLARE allo shim. Uso: ffi_ext_shim.pl <file.inc> <shim.c> <libdirs>...
use strict; use warnings; use File::Find;
my ($inc, $shimfile, @dirs) = @ARGV;
open(my $fh, '<', $inc) or die; my @lines = <$fh>; close $fh;
sub split_top { my ($s) = @_; my @o; my ($d, $cur) = (0, '');
  for my $ch (split //, $s) { $d++ if $ch eq '('; $d-- if $ch eq ')';
    if ($ch eq ',' && $d == 0) { push @o, $cur; $cur = ''; } else { $cur .= $ch } }
  push @o, $cur if length($cur) || @o; return @o; }
my @files; find(sub { push @files, $File::Find::name if /\.(c|h)$/ }, @dirs);
my %src; for my $f (@files) { open(my $c, '<', $f) or next; local $/; $src{$f} = <$c>; close $c; }
my (%shim, %hdr, @out, $changed, %shimfile_of);
for my $l (@lines) {
  if ($l =~ /^(.*DECLARE\s+(?:FUNCTION|SUB)\s+(\w+)\s+LIB\s+"[^"]+")(?:\s+ALIAS\s+"([^"]+)")?(\s*\((.*)\)\s*(?:AS\s+\w+)?\s*)$/i) {
    my ($pre, $name, $alias, $post, $params) = ($1, $2, $3, $4, $5); $post =~ s/\s+$//; my $sym = defined $alias ? $alias : $name; my $regen = ($ENV{REGEN} && $sym =~ s/__ffi$//) ? 1 : 0;
    my @ps = split_top($params); my @arr = grep { $ps[$_] =~ /^\s*(?:BYREF\s+|BYVAL\s+)?\w+\(\)\s+AS/i } 0..$#ps;
    if (@arr && ($ENV{FFI_SHIM_ALL} || $sym !~ /^ofx_/) && ($regen || $sym !~ /__ffi$/)) {
      my $found;
      for my $f (sort keys %src) {
        next unless $f =~ /\.c$/;
        if ($src{$f} =~ /(?:^|\n)((?:const\s+)?[A-Za-z_][\w\s\*]*?)\b\Q$sym\E\s*\(([^)]*)\)\s*\{/) { $found = [$f, $1, $2]; last; }
      }
      if ($found) {
        my ($f, $rt, $cp) = @$found; $rt =~ s/\s+$//; $rt =~ s/^\s+//;
        my @cps = split_top($cp);
        if (@cps == @ps && $rt !~ /\bstatic\b/) {
          my (@sig, @args);
          for my $k (0..$#cps) {
            my $q = $cps[$k]; $q =~ s/^\s+|\s+$//g; my ($nm) = $q =~ /(\w+)\s*(?:\[\s*\w*\s*\])?$/;
            unless (defined $nm && $q =~ /\s/ || $q =~ /\*\s*\w+$/) { $found = undef; last; }
            push @sig, $q; push @args, $nm;
            if (grep { $_ == $k } @arr) { push @sig, "int64_t ${nm}_ext"; }
          }
          if ($found) {
            my $ret = $rt eq 'void' ? '' : 'return ';
            $shimfile_of{$sym} = $f; $shim{$sym} = sprintf("%s %s__ffi(%s) { %s%s(%s); }\n", $rt, $sym, join(', ', @sig), $ret, $sym, join(', ', @args));
            # header che dichiara sym
            for my $h (sort keys %src) { next unless $h =~ /\.h$/; if ($src{$h} =~ /\b\Q$sym\E\s*\(/) { (my $b = $h) =~ s|.*/||; $hdr{$b} = 1; last; } }
            my $newsym = "${sym}__ffi";
            $l = $regen ? $l : defined $alias ? "$pre ALIAS \"$newsym\"$post\n" : "$pre ALIAS \"$newsym\"$post\n"; $l =~ s/\)\s*\)/)/ if 0;
            $changed++;
          }
        }
      }
      print STDERR "NOSHIM $sym\n" unless $shim{$sym};
    }
  }
  push @out, $l;
}
if (%shim) {
  my %by; push @{$by{$shimfile_of{$_}}}, $_ for keys %shim;
  for my $cf (sort keys %by) { my @todo = grep { $src{$cf} !~ /\b\Q$_\E__ffi\s*\(/ } sort @{$by{$cf}}; next unless @todo; open(my $o, ">>", $cf) or die; print $o "\n#include <stdint.h>\n/* Shim FFI (estensioni automatiche degli array, ffi_ext_shim.pl): firma con int64_t dopo ogni array. */\n"; print $o $shim{$_} for @todo; close $o; }
  unless ($ENV{REGEN}) { open(my $w, ">", $inc) or die; print $w @out; close $w; }
}
printf "shim creati %d\n", scalar(keys %shim);
