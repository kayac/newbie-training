# -*- coding: utf-8 -*-
require 'spec_helper'

# 必要なパッケージがインストールされていること
%w{
   epel-release
   telnet nc iotop dstat vim-enhanced ack mlocate sysstat
   file lsof git mosh ngrep bind-utils strace
}.each do |pkg|
  describe pkg do
    it { should be_installed }
  end
end

# 不要なサービスが停止していること
%w{ iptables ip6tables }.each do |service|
  describe service do
    it { should_not be_running }
  end
end

# ntp の設定
describe 'ntp' do
  it { should be_installed }
end

describe 'ntpd' do
  it { should be_running }
  it { should be_enabled }
end

describe '/etc/ntp.conf' do
  it { should be_file }
  it { should contain '^server ntp.nict.jp' }
  it { should_not contain '^server.*pool\.ntp\.org' }
end

# タイムゾーンの設定
describe '/etc/localtime' do
  it { should be_linked_to '/usr/share/zoneinfo/Japan' }
end

describe '/etc/sysconfig/clock' do
  it { should be_file }
  it { should contain 'ZONE="Asia/Tokyo"' }
end

# app ユーザについて
describe 'app' do
  it { should be_user }
end

describe '/home/app' do
  it { should be_directory }
  it { should be_mode '700' }
  it { should be_owned_by 'app' }
end


# nginx で webサーバが動作していて、コンテンツを返すこと
describe 'port 8000' do
  it { should be_listening }
end

describe 'nginx' do
  it { should be_installed }
  it { should be_running }
  it { should be_enabled }
end

describe "curl -i http://127.0.0.1:8000/" do
  it { should get_stdout "Hello World" }
  it { should get_stdout "HTTP/1.1 200 OK" }
end
