#!/usr/bin/env perl
use strict;
use warnings;
use 5.12.0;
use Parallel::Benchmark;
use Capture::Tiny qw/ :all /;
use Log::Minimal;
use IO::File;
use File::Temp qw/ tempfile /;

my @static = qw|
    /static/js/jquery-1.9.0.min.js
    /static/js/main.js
    /static/css/main.css
    /static/bootstrap/css/bootstrap.min.css
    /static/bootstrap/img/glyphicons-halflings-white.png
    /static/bootstrap/img/glyphicons-halflings.png
|;

my $timeout = $ENV{TIMEOUT} || 10;
$ENV{TIMEOUT} = $timeout;
my $branch = $ARGV[0];
my $target = do "config.pl";
$ENV{TARGET_HOST} = $target->{$branch} or die "no TARGET_HOST by branch: $branch";

my $run_checker = sub {
    # checker
    my ($stdout, $stderr, @result) = capture {
        system("prove", "t/checker.t");
    };
    if ( $stdout =~ /Result: PASS/ ) {
        debugf "checker Result: PASS";
        if ( $stderr =~ /TotalRequests: ([0-9]+)/ ) {
            my $score = $1;
            debugf "checker Score: %d", $score;
            return $score;
        }
    }
    warn $stdout;
    warn $stderr;
    0;
};
my $run_http_load = sub {
    # http_load
    my ($fh, $filename) = tempfile();
    for my $path (@static) {
        my $uri = "http://$ENV{TARGET_HOST}$path";
        $fh->print("$uri\n");
    }
    $fh->close;
    my ($stdout, $stderr, @result) = capture {
        system("http_load", "-parallel", 5, "-seconds", $timeout, $filename);
    };
    unlink $filename;
    debugf "http_load %s", $stdout;
    if ( $stdout =~ /^  code [345]\d\d/ ) {
        print "Result: FAIL\n";
        warn $stdout;
        warn $stderr;
        return 0;
    }

    if ( $stdout =~ /^([0-9]+) fetches,/ ) {
        my $score = $1;
        return $score / 100;
    }
    warn $stdout;
    warn $stderr;
    0;
};

my $run_furl_loader = sub {
    my ($stdout, $stderr, @result) = capture {
        system("prove", "t/furl_loader.t");
    };
    if ( $stdout =~ /Result: PASS/ ) {
        debugf "loader Result: PASS";
        if ( $stderr =~ /TotalRequests: ([0-9]+)/ ) {
            my $score = $1;
            debugf "loader Score: %d", $score;
            return $score;
        }
    }
    warn $stdout;
    warn $stderr;
    0;
};

my $pm = Parallel::Benchmark->new(
    time        => $timeout,
    concurrency => $ENV{C} || 5,
    benchmark   => sub {
        my ($self, $n) = @_;
        $n == 1 ? $run_checker->()
      : $n == 2 ? $run_http_load->()
      :           $run_furl_loader->();
    },
);
my $result;
my ($stdout, $stderr, @result) = capture {
    $result = $pm->run;
};
print "SCORE: $result->{score}\n";
print $stdout;
warn $stderr;
if ($stdout =~ /Result: FAIL/ || $stderr =~ /Result: FAIL/) {
    die;
}
else {
    exit;
}

