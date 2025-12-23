let SessionLoad = 1
let s:so_save = &g:so | let s:siso_save = &g:siso | setg so=0 siso=0 | setl so=-1 siso=-1
let v:this_session=expand("<sfile>:p")
silent only
silent tabonly
cd ~/programming/luvie
if expand('%') == '' && !&modified && line('$') <= 1 && getline(1) == ''
  let s:wipebuf = bufnr('%')
endif
let s:shortmess_save = &shortmess
if &shortmess =~ 'A'
  set shortmess=aoOA
else
  set shortmess=aoO
endif
badd +244 testiced/src/main.rs
badd +63 ~/.cargo/registry/src/index.crates.io-1949cf8c6b5b557f/iced_core-0.14.0/src/renderer.rs
badd +6 ~/.cargo/registry/src/index.crates.io-1949cf8c6b5b557f/iced_core-0.14.0/src/background.rs
badd +298 ~/.cargo/registry/src/index.crates.io-1949cf8c6b5b557f/iced_widget-0.14.2/src/rule.rs
badd +29 ~/.cargo/registry/src/index.crates.io-1949cf8c6b5b557f/iced_core-0.14.0/src/element.rs
badd +18 ~/.cargo/registry/src/index.crates.io-1949cf8c6b5b557f/iced_renderer-0.14.0/src/lib.rs
argglobal
%argdel
edit testiced/src/main.rs
argglobal
balt ~/.cargo/registry/src/index.crates.io-1949cf8c6b5b557f/iced_core-0.14.0/src/renderer.rs
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
let s:l = 244 - ((48 * winheight(0) + 25) / 50)
if s:l < 1 | let s:l = 1 | endif
keepjumps exe s:l
normal! zt
keepjumps 244
normal! 038|
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
