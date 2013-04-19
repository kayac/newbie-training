use strict;
use warnings;

my $app = sub {
    my $env = shift;

    # your webapp code here

    [ 200, ["Content-Type" => "text/html; charset=UTF-8"], ["Hello World"] ];
};
