#!/bin/sh

if [ -f $HOME/.dwmzrc ]; then
  . $HOME/.dwmzrc
elif [ -f /etc/dwmzrc ]; then
  . /etc/dwmzrc
fi

exec dwmz

