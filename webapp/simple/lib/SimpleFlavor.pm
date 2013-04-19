use strict;
use warnings;
use utf8;

package SimpleFlavor;
use parent qw/Amon2::Setup::Flavor/;
use Amon2::Lite;
use Amon2;

sub run {
    my ($self) = @_;

    $self->{amon2_version}      = $Amon2::VERSION;
    $self->{amon2_lite_version} = $Amon2::Lite::VERSION;

    $self->write_file('app.psgi', <<'...');
use strict;
use warnings;
use utf8;
use File::Spec;
use File::Basename;
use lib File::Spec->catdir(dirname(__FILE__), 'extlib', 'lib', 'perl5');
use lib File::Spec->catdir(dirname(__FILE__), 'lib');
use Amon2::Lite;
use Plack::Session::Store::File;

our $VERSION = '0.01';

# put your configuration here
sub load_config {
    my $c = shift;

    my $mode = $c->mode_name || 'development';

    +{
        'DBI' => [
            'dbi:SQLite:dbname=$mode.db',
            '',
            '',
        ],
    }
}

get '/' => sub {
    my $c = shift;
    return $c->render('index.tt');
};

# load plugins
# __PACKAGE__->load_plugin('Web::CSRFDefender');
# __PACKAGE__->load_plugin('DBI');
# __PACKAGE__->load_plugin('Web::FillInFormLite');
# __PACKAGE__->load_plugin('Web::JSON');

__PACKAGE__->enable_session(
    store => Plack::Session::Store::File->new( dir => "./tmp" ),
);

__PACKAGE__->to_app(handle_static => 1);

__DATA__

@@ index.tt
<!doctype html>
<html>
<head>
    <meta charset="utf-8">
    <title><% $module %></title>
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <script type="text/javascript" src="http://ajax.googleapis.com/ajax/libs/jquery/1.8.0/jquery.min.js"></script>
    <script type="text/javascript" src="[% uri_for('/static/js/main.js') %]"></script>
    <link rel="stylesheet" href="[% uri_for('/static/css/main.css') %]">
</head>
<body>
    <div class="container">
        <header><h1><% $module %></h1></header>
        <section class="row">
            This is a <% $module %>
        </section>
        <footer>Powered by <a href="http://amon.64p.org/">Amon2::Lite</a></footer>
    </div>
</body>
</html>

@@ /static/js/main.js

@@ /static/css/main.css
footer {
    text-align: right;
}
...

    $self->write_file('cpanfile', <<'...');
requires 'Amon2', '>=<% $amon2_version %>';
requires 'Amon2::Lite', '>=<% $amon2_lite_version %>';
requires 'Text::Xslate', '>=1.5006';
requires 'Plack::Session', '>=0.14';
...

    $self->write_file('t/Util.pm', <<'...');
package t::Util;
BEGIN {
    unless ($ENV{PLACK_ENV}) {
        $ENV{PLACK_ENV} = 'test';
    }
}
use parent qw/Exporter/;
use Test::More 0.96;

our @EXPORT = qw//;

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

1;
...

    $self->write_file('t/webapp.t', <<'...');
use strict;
use warnings;
use Plack::Test;
use Test::More;
use HTTP::Request;

my $app = require "app.psgi";
isa_ok $app, "CODE";

test_psgi
    app    => $app,
    client => sub {
        my $cb  = shift;
        my $req = HTTP::Request->new(
            GET => "http://localhost/",
        );
        my $res = $cb->($req);
        is $res->code, 200, "code";
        is $res->header("Content-Type") => "text/html; charset=UTF-8", "header";
        like $res->content, qr{<title>form</title>}, "title";
        like $res->content, qr{Hello World}, "text";
    };

test_psgi
    app    => $app,
    client => sub {
        my $cb  = shift;
        my $req = HTTP::Request->new(
            GET => "http://localhost/result?name=Mr%2E%20FooBar",
        );
        my $res = $cb->($req);
        is $res->code, 200, "code";
        is $res->header("Content-Type") => "text/html; charset=UTF-8", "header";
        like $res->content, qr{<title>result</title>}, "title";
        like $res->content, qr{Hello Mr\. FooBar}, "text";
    };

test_psgi
    app    => $app,
    client => sub {
        my $cb  = shift;
        my $req = HTTP::Request->new(
            GET => "http://localhost/xxx",
        );
        my $res = $cb->($req);
        is $res->code, 404, "code";
    };

done_testing;
...

    $self->write_file('tmp/.gitkeep', "");
}

1;
__END__

=head1 NAME

SimpleFlavor - Amon2::Lite based more simple flavor

=head1 SYNOPSIS

    % amon2-setup.pl --flavor=+SimpleFlavor MyApp

=head1 DESCRIPTION

This is a flavor for project using Amon2::Lite.

=head1 AUTHOR

Fujiwara Shunichiro
