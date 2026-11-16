# ffi_sigcheck.pl — confronta l'arità delle DECLARE con la definizione C (gate FFI).
#
# Uso: perl tools/sh/ffi_sigcheck.pl <file.inc> <dir>...
# Per ogni DECLARE con parametri ARRAY l'arità attesa cresce di 1 (le estensioni viaggiano
# dopo il puntatore): se il C dichiara un numero diverso di parametri, è drift di firma.
use strict; use warnings; use File::Find;
my ($inc, @dirs) = @ARGV; open(my $fh,'<',$inc) or die; my @lines=<$fh>; close $fh;
sub split_top { my ($s)=@_; my @o; my ($d,$cur)=(0,''); for my $ch (split //,$s) { $d++ if $ch eq '('; $d-- if $ch eq ')'; if ($ch eq ',' && $d==0) { push @o,$cur; $cur='' } else { $cur.=$ch } } push @o,$cur if length($cur)||@o; return @o; }
my @f; find(sub { push @f,$File::Find::name if /\.c$/ }, @dirs); my %src; for my $x (@f) { open(my $c,'<',$x) or next; local $/; $src{$x}=<$c>; close $c; }
my ($ok,$bad,$nf)=(0,0,0);
for my $l (@lines) {
  next unless $l =~ /DECLARE\s+(NATIVE\s+)?(?:FUNCTION|SUB)\s+(\w+)\s+LIB\s+"[^"]+"(?:\s+ALIAS\s+"([^"]+)")?\s*\((.*)\)\s*(?:AS\s+\w+)?\s*$/i;
  my ($nat,$name,$alias,$p)=($1,$2,$3,$4); my $sym=$alias//$name;
  my @ps = split_top($p); my $arr = grep { /^\s*(?:BYREF\s+|BYVAL\s+)?\w+\(\)\s+AS/i } @ps; next unless $arr;
  my $exp = $nat ? scalar(@ps) : @ps + $arr; my $found;
  for my $x (sort keys %src) { if ($src{$x} =~ /(?:^|\n)[A-Za-z_][\w\s\*]*?\b\Q$sym\E\s*\(([^)]*)\)\s*\{/) { my @cp = split_top($1); $found = scalar(@cp); last; } }
  if (!defined $found) { $nf++; print "NOTFOUND $sym\n"; next; }
  if ($found == $exp) { $ok++ } else { $bad++; print "MISMATCH $sym: DECLARE+estensioni=$exp, C=$found\n"; }
}
print "ok=$ok mismatch=$bad notfound=$nf\n";
