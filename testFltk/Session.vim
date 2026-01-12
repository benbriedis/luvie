let SessionLoad = 1
let s:so_save = &g:so | let s:siso_save = &g:siso | setg so=0 siso=0 | setl so=-1 siso=-1
let v:this_session=expand("<sfile>:p")
silent only
silent tabonly
cd ~/programming/luvie/testFltk
if expand('%') == '' && !&modified && line('$') <= 1 && getline(1) == ''
  let s:wipebuf = bufnr('%')
endif
let s:shortmess_save = &shortmess
if &shortmess =~ 'A'
  set shortmess=aoOA
else
  set shortmess=aoO
endif
badd +83 src/main.rs
badd +70 src/gridArea/grid.rs
badd +104 ~/programming/luvie/testui/grid.cpp
badd +0 Cargo.toml
badd +924 ~/.cargo/registry/src/index.crates.io-1949cf8c6b5b557f/fltk-1.5.22/src/macros/widget.rs
badd +490 ~/programming/luvie/testiced/src/gridArea/grid.rs
badd +57 ~/.cargo/registry/src/index.crates.io-1949cf8c6b5b557f/fltk-1.5.22/src/app/event.rs
argglobal
%argdel
$argadd Cargo.toml
edit src/gridArea/grid.rs
argglobal
balt ~/.cargo/registry/src/index.crates.io-1949cf8c6b5b557f/fltk-1.5.22/src/app/event.rs
setlocal foldmethod=manual
setlocal foldexpr=0
setlocal foldmarker={{{,}}}
setlocal foldignore=#
setlocal foldlevel=0
setlocal foldminlines=1
setlocal foldnestmax=20
setlocal foldenable
silent! normal! zE
let &fdl = &fdl
let s:l = 70 - ((12 * winheight(0) + 20) / 40)
if s:l < 1 | let s:l = 1 | endif
keepjumps exe s:l
normal! zt
keepjumps 70
normal! 065|
tabnext 1
if exists('s:wipebuf') && len(win_findbuf(s:wipebuf)) == 0 && getbufvar(s:wipebuf, '&buftype') isnot# 'terminal'
  silent exe 'bwipe ' . s:wipebuf
endif
unlet! s:wipebuf
set winheight=1 winwidth=20
let &shortmess = s:shortmess_save
let s:sx = expand("<sfile>:p:r")."x.vim"
if filereadable(s:sx)
  exe "source " . fnameescape(s:sx)
endif
let &g:so = s:so_save | let &g:siso = s:siso_save
set hlsearch
doautoall SessionLoadPost
unlet SessionLoad
" vim: set ft=vim :
