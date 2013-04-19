# -*- coding: utf-8 -*-
require 'spec_helper'

describe "/tmp/example.file" do
  it { should be_file }
  it { should be_mode 600 }
  it { should be_owned_by "root" }
  it { should contain "Hello World" }
end

