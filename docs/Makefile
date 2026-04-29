# romHEX 14 — User Manual build pipeline (local-only)
#
# Targets:
#   make install     - install Python deps into a venv
#   make gen         - run generators (glossary, menu ref, changelog)
#   make screenshots - capture screenshots in EN and zh-CN
#   make translate   - run Claude EN→zh-CN translation pass on diffs
#   make site        - build mkdocs HTML site (en + zh)
#   make pdf         - build per-language PDFs
#   make docs        - full pipeline: gen + translate + site + pdf
#   make serve       - mkdocs live preview on http://127.0.0.1:8000
#   make publish     - copy site/ + pdf/ into the public docs repo and push
#   make clean       - remove site/ and pdf/

PYTHON      ?= python
VENV        ?= .venv
PIP         := $(VENV)/Scripts/pip
MKDOCS      := $(VENV)/Scripts/mkdocs
PY          := $(VENV)/Scripts/python

# Path to the public docs repo working copy on this machine.
# Override on the command line: `make publish PUBLIC_REPO=/c/path/to/repo`
PUBLIC_REPO ?= ../../romHEX14-ECU-Tuning

.PHONY: install gen screenshots translate site pdf docs serve publish clean

install:
	$(PYTHON) -m venv $(VENV)
	$(PIP) install --upgrade pip
	$(PIP) install -r requirements.txt

gen:
	$(PY) tooling/gen_docs.py \
		--ts-en ../translations/rx14_en.ts \
		--ts-zh ../translations/rx14_zh_CN.ts \
		--src-dir ../src \
		--repo-root .. \
		--out-en src/en \
		--out-zh src/zh

screenshots:
	$(PY) tooling/run_screenshots.py \
		--app ../build/Desktop_Qt_6_10_2_MinGW_64_bit-Debug/rx14.exe \
		--fixtures "C:/Users/Cris/Desktop/特调" \
		--out-en src/en/assets/screenshots \
		--out-zh src/zh/assets/screenshots

screenshots-en:
	$(PY) tooling/run_screenshots.py \
		--app ../build/Desktop_Qt_6_10_2_MinGW_64_bit-Debug/rx14.exe \
		--fixtures "C:/Users/Cris/Desktop/特调" \
		--out-en src/en/assets/screenshots \
		--out-zh src/zh/assets/screenshots \
		--locale en

screenshots-list:
	$(PY) tooling/run_screenshots.py \
		--app ../build/Desktop_Qt_6_10_2_MinGW_64_bit-Debug/rx14.exe \
		--fixtures "C:/Users/Cris/Desktop/特调" \
		--out-en src/en/assets/screenshots \
		--out-zh src/zh/assets/screenshots \
		--list

translate:
	$(PY) tooling/translate.py \
		--src src/en \
		--dst src/zh \
		--model claude-opus-4-6 \
		--glossary src/zh/appendix-c-glossary.md

site:
	$(MKDOCS) build --strict

pdf:
	ENABLE_PDF_EXPORT=1 $(MKDOCS) build

docs: gen translate site pdf

serve:
	$(MKDOCS) serve

publish: docs
	@echo ">>> Publishing to $(PUBLIC_REPO)"
	rm -rf $(PUBLIC_REPO)/docs
	mkdir -p $(PUBLIC_REPO)/docs
	cp -r site/* $(PUBLIC_REPO)/docs/
	mkdir -p $(PUBLIC_REPO)/docs/pdf
	cp pdf/*.pdf $(PUBLIC_REPO)/docs/pdf/ 2>/dev/null || true
	@echo ">>> Done. Review and commit in $(PUBLIC_REPO)."

clean:
	rm -rf site pdf
