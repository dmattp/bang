; See http://www.emacswiki.org/emacs/ModeTutorial

(defvar bang-mode-hook nil)

(defvar bang-mode-map
  (let ((map (make-keymap)))
    (define-key map "\C-j" 'newline-and-indent)
    map)
  "Keymap for Bang! major mode")


;;;###autoload
(add-to-list 'auto-mode-alist '("\\.bang\\'" . bang-mode))

(defconst bang-font-lock-keywords-1
  (list
;   '("\\<\\(as\\|def\\|f\\(?:alse\\|un!?\\)\\|true\\)\\>" . font-lock-keyword-face)
;   '("\\('\\w*'\\)" . font-lock-variable-name-face)
   )
  "Minimal highlighting expressions for Bang! mode")


(defconst bang-font-lock-keywords-2
  (append bang-font-lock-keywords-1
     (list
      ; use regexp-opt (below) to generate this string
      '("\\<def!? +\\(: *[[:alnum:]-_]+\\)\\>" 1 font-lock-function-name-face)
      '("\\<\\(as\\|def\\||f\\(?:alse\\|un!?\\)\\|true\\)\\>" . font-lock-keyword-face)
      '("\\<\\(catch\\|try\\|throw\\|lookup\\|bind\\)\\>" . font-lock-builtin-face)
;      '("\\[:space:]def\\[:space:]\\+(w\\+)\\[:space:]\\*\\\\:" . font-lock-function-name-face)
;      '("\\<\\(\\|try\\)\\>" . font-lock-function-name-face)
      '("\\([?:;]\\)" . font-lock-function-name-face)
      '("\\([{}]\\)" . font-lock-function-name-face)
      '("\\([()]\\)" . font-lock-dmp-subdued-keyword-face)
;      '("\\([!?:;()]\\)" . font-lock-dmp-subdued-keyword-face)
;      '("\\([{}]\\)" . font-lock-function-name-face)
      ))
  "scope / block delimiters for Bang! mode")


;(regexp-opt '("def" "fun!" "fun" "true" "false" "as" "try" "catch") t)
;(regexp-opt '("try" "catch") t)
;(regexp-opt '("[:space:]def[:space:]+(\w+)[:space:]*\\:") t)
;(regexp-opt '("{" "}") t)
;(insert "fooq")

(defvar bang-mode-syntax-table "Syntax table for bang-mode")

; this is some seriously wacky shinola
(setq bang-mode-syntax-table
  (let ((st (make-syntax-table)))
    (modify-syntax-entry ?_ "w" st)
    (modify-syntax-entry ?\' "\"") ; single quotes are strings
    (modify-syntax-entry ?- ". 124b" st) ; open comment
    (modify-syntax-entry ?\n "> b" st)   ; close line comment
    st
    ))

(defcustom bang-indent-level 2
  "Amount by which Bang! subexpressions are indented."
  :type 'integer
  :group 'lua)

(defun bang-indent-line ()
  "Indent current line as Bang! code"
  (interactive)
  (beginning-of-line)
;  (if (bobp) (indent-line-to 0))
)

(defun bang-mode ()
  "Major mode for Bang! language files"
  (interactive)
  (kill-all-local-variables)
  (set-syntax-table bang-mode-syntax-table)
  (use-local-map bang-mode-map)
  (set (make-local-variable 'font-lock-defaults) '(bang-font-lock-keywords-2))
;  (set (make-local-variable 'indent-line-function) 'bang-indent-line)
  (setq major-mode 'bang-mode)
  (setq mode-name "Bang!")
  (run-hooks 'bang-mode-hook))


(provide 'bang-mode)
