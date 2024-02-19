;;; talk-mode.el --- Minor mode for real-time speech-to-text

;;; Commentary:
;; This Emacs minor mode does real-time voice transcription.

;;; Code:

(define-minor-mode talk-mode
  "A minor mode for communicating with minigram."
  :lighter " 🎤"
  :keymap (let ((map (make-sparse-keymap)))
	    (define-key map (kbd "C-c C-t") 'talk-start-minigram)
	    (define-key map (kbd "C-c C-k") 'talk-kill-minigram)
	    map) 
  :global nil
  (if talk-mode
      (message "Talk mode enabled")
    (talk-kill-minigram)))

(defvar talk-minigram-process nil
  "The process running minigram.")

(defvar talk-minigram-log nil
  "The log of minigram output.")

(defvar talk-language-custom-type
  '(choice (const :tag "Czech" "cs")
	   (const :tag "Danish" "da")
	   (const :tag "Dutch" "nl")
	   (const :tag "English" "en")
	   (const :tag "Flemish" "nl-BE")
	   (const :tag "French" "fr")
	   (const :tag "German" "de")
	   (const :tag "Greek" "el")
	   (const :tag "Hindi" "hi")
	   (const :tag "Indonesian" "id")
	   (const :tag "Italian" "it")
	   (const :tag "Korean" "ko")
	   (const :tag "Norwegian" "no")
	   (const :tag "Polish" "pl")
	   (const :tag "Portuguese" "pt")
	   (const :tag "Russian" "ru")
	   (const :tag "Spanish" "es")
	   (const :tag "Swedish" "sv")
	   (const :tag "Turkish" "tr")
	   (const :tag "Ukrainian" "uk")))

(defcustom talk-primary-language "en"
  "The default language for speech-to-text."
  :type talk-language-custom-type
  :group 'talk)

(defcustom talk-secondary-language "sv"
  "The secondary language for speech-to-text."
  :type talk-language-custom-type
  :group 'talk)

(defun talk-start-minigram (arg)
  "Starts the minigram process."
  (interactive "P")
  (message "Starting minigram %s" arg)
  (let* ((language (if arg
		       talk-secondary-language
		     talk-primary-language))
	 (talk-minigram-process
	  (make-process 
	   :name "minigram" 
	   :buffer (generate-new-buffer-name "*minigram-output*")
	   :stderr (generate-new-buffer-name "*minigram-error*")
	   :command `("minigram" 
		      "--diarize=true"
		      ,(concat "--language=" language))
	   :noquery t
	   :connection-type nil
	   :filter 'talk-minigram-output-filter
	   :sentinel 'talk-minigram-sentinel))
	 (overlay (make-overlay (point) (point) (current-buffer) nil t)))
    (process-put talk-minigram-process 'interim-overlay overlay)
    (talk-configure-interim-overlay language)))

(defun talk-interim-overlay ()
  (process-get talk-minigram-process 'interim-overlay))

(defun talk-configure-interim-overlay (language)
  (let ((overlay (talk-interim-overlay)))
    (overlay-put overlay 'face 'italic)
    (let ((mic-emoji-with-smaller-font 
	   (propertize (concat "🎤" language) 'face '(:height 0.8))))
      (overlay-put overlay 'before-string "")
      (overlay-put overlay 'after-string mic-emoji-with-smaller-font))))

(defun talk-minigram-output-filter (process output)
  "Filter function for handling minigram output.
PROCESS is the process under which minigram is running.
OUTPUT is the output produced by minigram."
  (condition-case e
      (let ((talk-minigram-process process))
	(talk-minigram-handle-output output))
    (error
     (message "Error parsing minigram output: %s" output)
     (talk-stop)
     (signal (car e) (cdr e)))))

(defun talk-minigram-handle-output (output)
  (let ((buffer (process-buffer talk-minigram-process)))
    (when (buffer-live-p buffer)
      (with-current-buffer buffer
	(goto-char (point-max))
	(insert output)
	(when (string-match "\n" output)
	  (save-excursion
            (forward-line -1)
	    (talk-process-deepgram-event)))))))

(defun talk-process-deepgram-event ()
  (let* ((json-object-type 'plist)
	 (json-array-type 'list)
         (json-false nil)
         (json (json-read))
         (request-id (plist-get (plist-get json :metadata) 
				:request_id))
         (is-final (plist-get json :is_final))
         (result (car (plist-get (plist-get json :channel) 
				  :alternatives))))
    (talk-update-transcript request-id result is-final)
    (push json talk-minigram-log)))

(defvar talk-deepgram-result-example
  "Example result from Deepgram streaming endpoint."
  '(:type "Results" :channel_index (0 1) :duration 1.3300018 :start
	  50.67 :is_final t :speech_final t :channel
	  (:alternatives
	   ((:transcript
	     "but I don't know about that." :confidence 1.0
	     :words ((:word "but" :start 50.829998 :end 50.891666
			    :confidence 1.0 :speaker 0
			    :punctuated_word "but")
		     (:word "i" :start 50.891666 :end 51.23
			    :confidence 0.99902344 :speaker 0
			    :punctuated_word "I")
		     (:word "don't" :start 51.23 :end 51.469997
			    :confidence 0.9980469 :speaker 0
			    :punctuated_word "don't")
		     (:word "know" :start 51.469997 :end 51.629997
			    :confidence 1.0 :speaker 0
			    :punctuated_word "know")
		     (:word "about" :start 51.629997 :end 51.815
			    :confidence 1.0 :speaker 0
			    :punctuated_word "about")
		     (:word "that" :start 51.815 :end 52.0
			    :confidence 0.95532227 :speaker 0
			    :punctuated_word "that.")))))
	  :metadata
	  (:request_id "c4864a49-2c03-4d02-a4b8-b13a75115e09"
		       :model_info
		       (:name "2-general-nova" :version
			      "2024-01-18.26916" :arch "nova-2")
		       :model_uuid
		       "c0d1a568-ce81-4fea-97e7-bd45cb1fdf3c")))

(defface talk-confidence-sure
  '((t nil))
  "Face for high-confidence words."
  :group 'talk-mode)

(defface talk-confidence-high
  '((t :foreground "gray70"))
  "Face for high-confidence words."
  :group 'talk-mode)

(defface talk-confidence-medium
  '((t :foreground "gray60"))
  "Face for medium-confidence words."
  :group 'talk-mode)

(defface talk-confidence-low
  '((t :foreground "gray50"))
  "Face for low-confidence words."
  :group 'talk-mode)

(defface talk-speaker-0
  '((t nil))
  "Face for speaker 0."
  :group 'talk-mode)

(defface talk-speaker-1
  '((t :background "ivory4"))
  "Face for speaker 1."
  :group 'talk-mode)

(defface talk-speaker-2
  '((t :background "LightGoldenrod4"))
  "Face for speaker 2."
  :group 'talk-mode)

(defface talk-speaker-3
  '((t :background "DeepSkyBlue4"))
  "Face for speaker 3."
  :group 'talk-mode)

(defun talk-update-transcript (request-id result is-final)
  "Updates the transcript overlay with the given RESULT."
  (let ((overlay (process-get talk-minigram-process 'interim-overlay)))
    (unless overlay
      (error "No overlay found for request-id %s" request-id))
    (with-current-buffer (overlay-buffer overlay)
      (let ((was-at-overlay-end (eq (point) (overlay-end overlay))))
	(save-excursion
	  (goto-char (overlay-start overlay))
	  (delete-region (point) (overlay-end overlay))
	  (let ((words (plist-get result :words))
		(transcript (plist-get result :transcript)))
	    (dolist (word words)
	      (let* ((confidence (plist-get word :confidence))
		     (punctuated-word (plist-get word :punctuated_word))
		     (word-start (point)))
		(insert punctuated-word)
		(let ((word-overlay
		       (make-overlay word-start (point) (current-buffer) 
				     t nil))
		      (confidence-face
		       (cond
			((> confidence 0.95) 'talk-confidence-sure)
			((> confidence 0.9) 'talk-confidence-high)
			((> confidence 0.7) 'talk-confidence-medium)
			(t 'talk-confidence-low)))
		      (speaker-face
		       (cond
			((= (plist-get word :speaker) 0) 'talk-speaker-0)
			((= (plist-get word :speaker) 1) 'talk-speaker-1)
			((= (plist-get word :speaker) 2) 'talk-speaker-2)
			((= (plist-get word :speaker) 3) 'talk-speaker-3)
			(t nil))))
		  (overlay-put word-overlay 'face 
			       (append (list confidence-face) 
				       (when speaker-face
					 (list speaker-face)))))
		(insert " ")
		(when (string-match "[.!?]" punctuated-word)
		  (insert " "))))
	    (when is-final
	      (unless (string-equal transcript "")
		(when (string-match "[.!?]$" transcript)
		  (insert "\n")))
              (move-overlay overlay 
			    (overlay-end overlay) 
			    (overlay-end overlay)))))
	(when was-at-overlay-end
	  (goto-char (overlay-end overlay)))))))

(defun talk-kill-minigram ()
  "Kills the minigram processes in the current buffer."
  (interactive)
  (dolist (process (process-list))
    (when (and (string-match "minigram.*" (process-name process))
	       (process-get process 'interim-overlay)
	       (eq (current-buffer) 
		   (overlay-buffer (process-get process 'interim-overlay))))
      (delete-overlay (process-get process 'interim-overlay)) 
      (delete-process process))))

(defun talk-stop ()
  "Stops the talk mode."
  (interactive)
  (talk-mode -1))

(defun talk-minigram-sentinel (process event)
  "Sentinel function for handling minigram process events."
  (message "Minigram process event: %s" event)
  (when (not (process-live-p process))
    (talk-stop)))

(provide 'talk-mode)

;;; talk-mode.el ends here
