" Modified from https://github.com/42Paris/42header

let s:asciiart = [
     \"                    ",
     \"                    ",
     \"                    ",
     \"                    ",
     \"           .-*-.    ",
     \"         .'* *.'    ",
     \"      __/_*_*(_     ",
     \"     / _______ \\    ",
     \"     \\_)     (_/    ",
     \"                    ",
     \"                    ",
     \"                    ",
\]

let s:start     = '/*'
let s:end       = '*/'
let s:fill      = '*'
let s:length    = 80
let s:margin    = 5

let s:types = {
      \ '\.c$\|\.h$\|\.cu$\|\.cl$\|\.cc$\|\.hh$\|\.cpp$\|\.hpp$\|\.tpp$\|\.ipp$\|\.cxx$\|\.go$\|\.rs$\|\.php$\|\.py$\|\.java$\|\.kt$\|\.kts$': ['/*', '*/', '*'],
      \ '\.htm$\|\.html$\|\.xml$': ['<!--', '-->', '*'],
      \ '\.js$\|\.ts$': ['//', '//', '*'],
      \ '\.tex$': ['%', '%', '*'],
      \ '\.ml$\|\.mli$\|\.mll$\|\.mly$': ['(*', '*)', '*'],
      \ '\.vim$\|\vimrc$': ['"', '"', '*'],
      \ '\.el$\|\emacs$\|\.asm$': [';', ';', '*'],
      \ '\.f90$\|\.f95$\|\.f03$\|\.f$\|\.for$': ['!', '!', '/'],
      \ '\.lua$': ['--', '--', '-']
\}

function! s:filetype()
    let l:f = s:filename()

    let s:start = '#'
    let s:end   = '#'
    let s:fill  = '*'

    for type in keys(s:types)
        if l:f =~ type
            let s:start = s:types[type][0]
            let s:end   = s:types[type][1]
            let s:fill  = s:types[type][2]
        endif
    endfor
endfunction

function! s:ascii(n)
    return a:n >= 0 && a:n < len(s:asciiart) ? s:asciiart[a:n] : ''
endfunction

function! s:textline(left, right)
    let l:left = strpart(a:left, 0, s:length - s:margin * 2 - strlen(a:right))
    return s:start . repeat(' ', s:margin - strlen(s:start)) . l:left . repeat(' ', s:length - s:margin * 2 - strlen(l:left) - strlen(a:right)) . a:right . repeat(' ', s:margin - strlen(s:end)) . s:end
endfunction

function! s:created_date()

    let l:filename = expand('%')
    let l:raw = system("git log --diff-filter=A --follow --format=%aI -- " . shellescape(l:filename) . " | tail -1")

    if v:shell_error || empty(l:raw)
        return s:date()
    endif

    let l:raw = substitute(l:raw, '\n', '', '')

    " Convert '2024-07-10T10:59:00+02:00' -> '2024/07/10 10:59:00'
    let l:raw = substitute(l:raw, '-', '/', 'g')         " dashes to slashes
    let l:raw = substitute(l:raw, 'T', ' ', '')          " T to space
    let l:raw = substitute(l:raw, '\(+\|-\)\d\{2}:\d\{2}$', '', '') " remove timezone

    return l:raw
endfunction

function! s:original_author()
    let l:filename = expand('%')
    let l:cmd = "git log --diff-filter=A --follow --format='%an' -- " . shellescape(l:filename) . " | tail -1"
    let l:author = system(l:cmd)
    if v:shell_error || empty(l:author)
        return s:name()
    endif
    return substitute(l:author, '\n', '', '')
endfunction

function! s:line(n)
    if a:n == 1 || a:n == 14
        return s:start . ' ' . repeat(s:fill, s:length - strlen(s:start) - strlen(s:end) - 2) . ' ' . s:end
    elseif a:n == 2 || a:n == 13
        return s:textline('', '')
    elseif a:n == 3
        return s:textline(s:filename(), s:ascii(4))
    elseif a:n == 4
        return s:textline('', s:ascii(5))
    elseif a:n == 5
        return s:textline("Created: " . s:created_date() . " by " . s:original_author(), s:ascii(6))
    elseif a:n == 6
        return s:textline("Updated: " . s:date() . " by " . s:name(), s:ascii(7))
    elseif a:n == 7
        return s:textline('', s:ascii(8))
    elseif a:n == 8
        return s:textline("License: " . g:license, '')
    elseif a:n == 9
        return s:textline('', '')
    elseif a:n == 10
        return s:textline("Author: " . s:name() . " <" . s:mail() . ">", '')
    elseif a:n == 11
        return s:textline('', '')
    elseif a:n == 12
        return s:textline("Copyright: " . g:copyright, '')
    else
        return s:textline('', '')
    endif
endfunction

function! s:name()
    if exists('g:name') | return g:name | endif
    let l:name = $NAME
    return strlen(l:name) > 0 ? l:name : "set 'NAME' or 'g:name'"
endfunction

function! s:mail()
    if exists('g:mail') | return g:mail | endif
    let l:mail = $MAIL
    return strlen(l:mail) > 0 ? l:mail : "set 'MAIL' or 'g:mail'"
endfunction

function! s:filename()
    let l:filename = expand("%:t")
    return strlen(l:filename) > 0 ? l:filename : "< new >"
endfunction

function! s:date()
    return strftime("%Y/%m/%d %H:%M:%S")
endfunction

function! s:insert()
    let l:line = 14
    call append(0, "")
    while l:line > 0
        call append(0, s:line(l:line))
        let l:line -= 1
    endwhile
endfunction

function! s:update()
    call s:filetype()
    if getline(6) =~ s:start . repeat(' ', s:margin - strlen(s:start)) . "Updated: "
        if &mod
            call setline(6, s:line(6))
        endif
        call setline(3, s:line(3))
        return 0
    endif
    return 1
endfunction

function! s:stdheader()
    if s:update()
        call s:insert()
    endif
endfunction

command! Stdheader call s:stdheader()
map <F2> :Stdheader<CR>
autocmd BufWritePre * call s:update()

