#
# Cookbook Name:: example
# Recipe:: default
#
# Copyright 2013, YOUR_COMPANY_NAME
#
# All rights reserved - Do Not Redistribute
#

template "/tmp/example.file" do
  owner  "root"
  mode   0600
  source "example.file.erb"
end
