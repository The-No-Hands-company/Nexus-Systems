.PHONY: generate serve test verify dev

generate:
	python3 extract_graph.py --root . --out graph.json

serve:
	python3 -m http.server 8922

test:
	python3 -m unittest discover -s tests -p "test_*.py" -v

verify: generate test

dev: verify serve
