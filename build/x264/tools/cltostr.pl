# Perl script used for compiling OpenCL src into x264 binary
#
# Copyright (C) 2013-2014 x264 project
# Authors: Steve Borho <sborho@multicorewareinc.com>

use Digest::MD5 qw(md5_hex);

# xxd takes a VAR, which will be the variable name
# and BYTES, a string of bytes to beencoded.
sub xxd
{
  my %args = @_;
  my $var = $args{VAR};
  my $bytes = $args{BYTES};
  my @hexbytes;
  my @bytes = split //, $$bytes;
  foreach $b (@bytes)
  {
    push @hexbytes, sprintf("0x%02X", ord($b));
  }

  # Format 'em nice and pretty-like.
  print 'static const char ' . $var . '[] = {' . "\n";
  my $count = 0;
  foreach my $h (@hexbytes)
  {
    print "$h, ";
    $count++;
    if ($count == 16)
    {
      print "\n";
      $count = 0;
    }
  }
  print "\n0x00 };\n\n";

  return;
}

if (@ARGV < 1)
{
  printf "%s: VARNAME ", $0 . "\n";
  exit(-1);
}


my @lines;
while(<STDIN>)
{
  s/^\s+//;                # trim leading whitespace
  if (/^\/\//)
  {
    next;   # skip the line if it starts with '//'
  }
  push @lines, $_;
}

my $lines = join '', @lines;
xxd(VAR => @ARGV[0], BYTES => \$lines);

my $hash = md5_hex($lines);
@hash = ( $hash =~ m/../g );


xxd(VAR => @ARGV[0] . "_hash", BYTES => \$hash);
