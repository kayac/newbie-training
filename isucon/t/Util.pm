package t::Util;
use 5.12.0;

BEGIN {
    unless ($ENV{PLACK_ENV}) {
        $ENV{PLACK_ENV} = 'test';
    }
}
use parent qw/Exporter/;
use Test::More 0.96;
use Test::mysqld;
use Path::Class qw/ file /;
use Plack::Util;
use HTML::TreeBuilder::XPath;
use HTML::Selector::XPath 'selector_to_xpath';
use Log::Minimal;

our @EXPORT = qw/ setup findnodes subtest_psgi request total_requests /;
our $Furl;
our $TotalRequests = 0;
if ($ENV{TARGET_HOST}) {
    require Furl;
    $Furl = Furl->new(
        agent         => "kayac-isucon/0.1",
        max_redirects => 0,
        timeout       => 30,
    );
}

{
    # utf8 hack.
    binmode Test::More->builder->$_, ":utf8" for qw/output failure_output todo_output/;
    no warnings 'redefine';
    my $code = \&Test::Builder::child;
    *Test::Builder::child = sub {
        my $builder = $code->(@_);
        binmode $builder->output,         ":utf8";
        binmode $builder->failure_output, ":utf8";
        binmode $builder->todo_output,    ":utf8";
        return $builder;
    };
}

sub setup {
    return if $Furl;

    my $mysqld = Test::mysqld->new(
        my_cnf => {
            'skip-networking' => '', # no TCP socket
        }
    ) or plan skip_all => $Test::mysqld::errstr;
    my $dbh = DBI->connect(
        $mysqld->dsn( dbname => 'test' ),
    );
    $dbh->do($_) for ( split /\n\n/, scalar file("sql/nopaste.sql")->slurp );

    my $app = Plack::Util::load_psgi( $ENV{APP} || "app.psgi" );
    $NoPaste::DSN = [ $mysqld->dsn( dbname => 'test' ) ];

    ($app, $mysqld);
}

sub findnodes {
    my ($content) = @_;
    my $tree = HTML::TreeBuilder::XPath->new;
    $tree->parse($content);
    sub {
        my $selector = shift;
        $tree->findnodes( selector_to_xpath($selector) );
    };
}

sub subtest_psgi {
    my ($name, $app, $client) = @_;
    if ($Furl) {
        Test::More::subtest $name => sub {
            $client->(sub { $Furl->request(shift) });
        };
    }
    else {
        Test::More::subtest $name => sub {
            Plack::Test::test_psgi app => $app, client => $client;
        };
    }
}

sub request {
    my ($cb, $req) = @_;

    state $cookie;

    $req->uri->host($ENV{TARGET_HOST}) if $ENV{TARGET_HOST};
    $req->header("Cookie" => $cookie) if defined $cookie;
    $req->header("Accept-Encoding" => "gzip,deflate");
    note $req->as_string if $ENV{DEBUG};
    my $res = $cb->($req);
    $TotalRequests++;

    if ( my $line = $res->header("Set-Cookie") ) {
        my ($name, $value) = split /[ ;=]/, $line;
        $cookie = "$name=$value";
    }
    $res;
}

sub total_requests { $TotalRequests }

1;
