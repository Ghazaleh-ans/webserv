#!/usr/bin/perl
# Perl CGI type. Selected by `cgi_extension .pl /usr/bin/perl;`.
use strict;
use warnings;

my $query = defined $ENV{'QUERY_STRING'} ? $ENV{'QUERY_STRING'} : '';

print "Content-Type: application/json\r\n";
print "\r\n";
print "{\"interpreter\":\"perl\",\"query\":\"$query\",\"time\":\"" . localtime() . "\"}\n";
