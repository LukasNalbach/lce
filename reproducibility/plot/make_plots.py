#!/usr/bin/env python3
"""Regenerates the data of the paper's table and figures from RESULT files.

The figures of the paper are pgfplots pictures (plot/paper/plots/*.tex) whose
data lines were produced with sqlplot-tools
(https://github.com/bingmann/sqlplot-tools): every data block is preceded by
the SQL statement it stems from, written as a comment of the form

    %% MULTIPLOT(algo)
    %% SELECT algo, <expr> AS x, <expr> AS y FROM results WHERE ...

This script evaluates exactly these statements (with SQLite, like
sqlplot-tools) on the RESULT lines printed by the benchmark tools and rewrites
the data lines that follow them. Layout, axis options, plot styles and
captions of the plot sources are left untouched. Three presentation rules that
were applied by hand in the paper are stated explicitly below (MEM_PEAK_FILES,
INPLACE_ALGOS, PRED_MIN_MEMORY).

Only the Python standard library is required.
"""

import argparse
import datetime
import math
import os
import re
import socket
import sqlite3
import sys

# In these figures every data structure is drawn with two marks at the same
# construction time: its final size (c_mem) and its construction memory peak
# (c_mempeak). The plot sources contain one SQL statement for each; their
# results are merged into a single plot.
MEM_PEAK_FILES = {"sss-construction.tex", "construction.tex"}

# fp-64 and rk-prezza overwrite the text and are regarded as in-place by the
# paper. Fig. 6 therefore draws them with a single mark at the bottom of its
# logarithmic memory axis instead of at their measured memory usage.
INPLACE_FILES = {"construction.tex"}
INPLACE_ALGOS = {"fp64", "rk-prezza"}
INPLACE_MEMORY = 0.01

# Fig. 2 has a logarithmic memory axis whose lowest tick (10^-4) is labelled
# "0": successor data structures without any extra memory (binary search) are
# drawn there.
PRED_FILES = {"predecessor-avg.tex"}
PRED_MIN_MEMORY = 0.0001

# Table 1: the texts in the order of the paper, grouped as in the paper.
TABLE_GROUPS = [
    ("non-repetitive", [["wiki.txt", "dna.txt", "cc.txt"],
                        ["dblp.xml", "dna", "english", "proteins", "sources"]]),
    ("repetitive", [["cere", "coreutils", "Escherichia_Coli", "einstein.de.txt",
                     "einstein.en.txt", "influenza", "kernel"]]),
]
TAUS = [256, 512, 1024, 2048]

IMPORT_RE = re.compile(r"^\s*%+\s*IMPORT-DATA\s+(\S+)\s+(\S+)")
DIRECTIVE_RE = re.compile(r"^\s*%%(.*)$")
MULTIPLOT_RE = re.compile(r"^\s*-?\s*MULTIPLOT\(([^)]*)\)(.*)$", re.S)
PLOT_RE = re.compile(r"^\s*%?\s*\\addplot(?:\[([^\]]*)\])?")
LEGEND_RE = re.compile(r"^\s*%?\s*\\addlegendentry\{(.*)\}\s*$")
PIE_RE = re.compile(r"^(\s*)[0-9.eE+-]+(/\w+/[^/]+/[^/,]+,?\s*)$")
PIE_BEGIN_RE = re.compile(r"^\s*\\begin\{tikzpicture\}\[pie chart\]")
PIE_END_RE = re.compile(r"^\s*\\end\{tikzpicture\}")

warnings = []


def warn(message):
    warnings.append(message)
    print("warning: " + message, file=sys.stderr)


# --------------------------------------------------------------------------
# RESULT files -> SQLite tables (the IMPORT-DATA command of sqlplot-tools)
# --------------------------------------------------------------------------

def read_results(path):
    """Returns the key=value pairs of all RESULT lines of a file."""
    rows = []
    if not os.path.isfile(path):
        return rows
    with open(path, errors="replace") as lines:
        for line in lines:
            pos = line.find("RESULT")
            if pos < 0:
                continue
            row = {}
            for item in line[pos + len("RESULT"):].split():
                key, sep, value = item.partition("=")
                if sep:
                    row[key] = value
            rows.append(row)
    return rows


def convert(value, kind):
    if value is None:
        return None
    return {"INTEGER": int, "REAL": float, "TEXT": str}[kind](value)


def import_table(db, name, rows):
    """Creates a table with one column per key; numeric columns get a numeric
    type so that SQL arithmetic behaves as in sqlplot-tools."""
    columns = {}
    for row in rows:
        for key, value in row.items():
            kinds = columns.setdefault(key, set())
            if re.fullmatch(r"[+-]?\d+", value) and abs(int(value)) < 2 ** 63:
                kinds.add("INTEGER")
            else:
                try:
                    float(value)
                    kinds.add("REAL")
                except ValueError:
                    kinds.add("TEXT")
    types = {}
    for key, kinds in columns.items():
        types[key] = ("TEXT" if "TEXT" in kinds
                      else "REAL" if "REAL" in kinds else "INTEGER")
    db.execute(f'DROP TABLE IF EXISTS "{name}"')
    if not types:
        db.execute(f'CREATE TABLE "{name}" (no_data)')
        return
    db.execute('CREATE TABLE "{}" ({})'.format(
        name, ", ".join(f'"{key}" {kind}' for key, kind in types.items())))
    keys = list(types)
    db.executemany(
        'INSERT INTO "{}" VALUES ({})'.format(name, ", ".join("?" * len(keys))),
        ([convert(row.get(key), types[key]) for key in keys] for row in rows))


def query(db, sql):
    """Runs a statement; returns (column names, rows). A statement that refers
    to data that has not been measured yields no rows."""
    try:
        cursor = db.execute(sql)
    except sqlite3.OperationalError as error:
        if "no such column" in str(error) or "no such table" in str(error):
            return [], []
        raise
    return [column[0] for column in cursor.description], cursor.fetchall()


# --------------------------------------------------------------------------
# Plot sources
# --------------------------------------------------------------------------

def number(value):
    """Formats a coordinate like sqlplot-tools (six significant digits)."""
    if isinstance(value, float) and value == int(value) and abs(value) < 1e6:
        return "%.1f" % value
    return "%g" % value


def pie_value(value):
    """Formats a pie slice like the paper: truncated to four significant
    digits."""
    if value <= 0:
        return "0"
    factor = 10 ** (3 - math.floor(math.log10(value)))
    return format(math.floor(value * factor + 1e-9) / factor, "#.4g").rstrip(".")


def unescape(text):
    return text.replace("\\_", "_")


def escape(text):
    return text.replace("_", "\\_")


def styles_of(lines):
    """Maps legend entries to the options of their \\addplot command."""
    styles = {}
    options = None
    for line in lines:
        plot = PLOT_RE.match(line)
        legend = LEGEND_RE.match(line)
        if plot:
            options = plot.group(1)
        elif legend and options is not None:
            styles.setdefault(unescape(legend.group(1)), options)
            options = None
    return styles


def multiplot_groups(db, columns_spec, sql):
    """Evaluates a MULTIPLOT statement. Returns the plots in order of
    appearance as (legend, group values, [(x, y), ...])."""
    group_columns = [column.strip() for column in columns_spec.split(",")]
    sql = re.sub(r"\bMULTIPLOT\b", ", ".join(group_columns), sql)
    names, rows = query(db, sql)
    if not rows:
        return []
    try:
        x_index, y_index = names.index("x"), names.index("y")
        group_indexes = [names.index(column) for column in group_columns]
    except ValueError:
        raise SystemExit(f"statement lacks an x, y or MULTIPLOT column: {sql}")
    groups = {}
    for row in rows:
        values = tuple(row[index] for index in group_indexes)
        points = groups.setdefault(values, [])
        if row[x_index] is not None and row[y_index] is not None:
            points.append((row[x_index], row[y_index]))
    result = []
    for values, points in groups.items():
        legend = ",".join(f"{column}={value}"
                          for column, value in zip(group_columns, values))
        result.append((legend, values, sorted(points)))
    return result


def fill_block(db, file_name, directive, old_lines, file_styles):
    """Returns the new data lines of one block and whether it has data."""
    statement = " ".join(DIRECTIVE_RE.match(line).group(1).strip()
                         for line in directive)
    match = MULTIPLOT_RE.match(statement)
    if not match:
        return old_lines, None  # not a data block
    columns_spec, sql = match.group(1), match.group(2).strip()

    # pie charts: one "value/slice/label offsets" line per statement
    if old_lines and PIE_RE.match(old_lines[0]):
        names, rows = query(db, sql)
        pie = PIE_RE.match(old_lines[0])
        if not rows or rows[0][names.index("y")] is None:
            return [f"{pie.group(1)}0{pie.group(2)}"], False
        value = pie_value(rows[0][names.index("y")])
        return [f"{pie.group(1)}{value}{pie.group(2)}"], True

    merge_peak = file_name in MEM_PEAK_FILES
    if merge_peak and "c_mempeak /" in sql:
        return [], None  # merged into the plots of the preceding c_mem block
    groups = multiplot_groups(db, columns_spec, sql)
    peaks = {}
    if merge_peak:
        peak_sql = sql.replace("c_mem /", "c_mempeak /")
        peaks = {values: points for _, values, points
                 in multiplot_groups(db, columns_spec, peak_sql)}

    local_styles = styles_of(old_lines)
    indent = re.match(r"\s*", (old_lines or directive)[0]).group(0)
    new_lines = []
    for legend, values, points in groups:
        if merge_peak:
            if file_name in INPLACE_FILES and values[0] in INPLACE_ALGOS:
                points = [(x, INPLACE_MEMORY) for x, _ in points]
            else:
                points = points + peaks.get(values, [])
        if file_name in PRED_FILES:
            points = [(x, max(y, PRED_MIN_MEMORY)) for x, y in points]
        if not points:
            continue
        if legend in local_styles:
            options = local_styles[legend]
        elif len(groups) == 1 and len(local_styles) == 1:
            options = next(iter(local_styles.values()))
        elif legend in file_styles:
            options = file_styles[legend]
        else:
            warn(f"{file_name}: '{legend}' is not part of the paper's figure "
                 "and is omitted")
            continue
        coordinates = " ".join(f"({number(x)},{number(y)})" for x, y in points)
        new_lines.append(f"{indent}\\addplot[{options}] coordinates "
                         f"{{ {coordinates} }};")
        new_lines.append(f"{indent}\\addlegendentry{{{escape(legend)}}}")
    return new_lines, bool(new_lines)


def is_data_line(line):
    return bool(PLOT_RE.match(line) or LEGEND_RE.match(line)
                or PIE_RE.match(line))


def fill_template(template_path, results_dir):
    """Returns the lines of a plot source with regenerated data."""
    file_name = os.path.basename(template_path)
    with open(template_path) as template:
        lines = template.read().split("\n")
    file_styles = styles_of(lines)
    db = sqlite3.connect(":memory:")
    output = []
    pie_start, pie_blocks, pie_filled = None, 0, 0
    position = 0
    while position < len(lines):
        line = lines[position]
        data_import = IMPORT_RE.match(line)
        if data_import:
            table, source = data_import.groups()
            path = os.path.join(results_dir, os.path.basename(source))
            if not os.path.isfile(path):
                warn(f"{file_name}: {path} does not exist")
            import_table(db, table, read_results(path))
        if PIE_BEGIN_RE.match(line):
            pie_start, pie_blocks, pie_filled = len(output), 0, 0
        if not DIRECTIVE_RE.match(line):
            output.append(line)
            position += 1
            if pie_start is not None and PIE_END_RE.match(line):
                if pie_blocks and not pie_filled:
                    output[pie_start:] = [
                        "  \\begin{tikzpicture}",
                        "    \\node[minimum width=4.2cm, minimum height=3cm, "
                        "text=gray, font=\\footnotesize] {no data};",
                        "  \\end{tikzpicture}"]
                pie_start = None
            continue
        directive = []
        while position < len(lines) and DIRECTIVE_RE.match(lines[position]):
            directive.append(lines[position])
            position += 1
        old_lines = []
        while position < len(lines) and is_data_line(lines[position]):
            old_lines.append(lines[position])
            position += 1
        new_lines, has_data = fill_block(db, file_name, directive, old_lines,
                                         file_styles)
        output.extend(directive)
        output.extend(new_lines)
        if has_data is not None:
            pie_blocks += 1
            pie_filled += bool(has_data)
    return output


# --------------------------------------------------------------------------
# Table 1
# --------------------------------------------------------------------------

def truncate(value, digits):
    """Formats like the paper's table, which truncates instead of rounding."""
    factor = 10 ** digits
    return f"{int(value * factor) / factor:.{digits}f}"


def format_size(gigabytes):
    return truncate(gigabytes, 1 if gigabytes >= 100 else 3)


def format_ratio(ratio):
    return "--" if ratio is None else truncate(ratio, 1 if ratio >= 10 else 2)


def table_rows(results_dir, stats_path):
    """Returns for every measured text a list of table rows
    (label, size, sigma, [ratio per tau], underlined)."""
    db = sqlite3.connect(":memory:")
    import_table(db, "results",
                 read_results(os.path.join(results_dir, "lce.results.new.par")))
    import_table(db, "stats", read_results(stats_path))
    rows = {}
    texts = [text for _, parts in TABLE_GROUPS for part in parts for text in part]
    for text in texts:
        measured = {}
        for tau in TAUS:
            _, result = query(db, "SELECT text_size, sss_size, sss_runs "
                              f"FROM results WHERE text = '{text}' "
                              f"AND algo = 'sss{tau}' AND threads = 1")
            if result:
                measured[tau] = result[0]
        if not measured:
            continue
        text_size = next(iter(measured.values()))[0]
        names, stats = query(db, f"SELECT * FROM stats WHERE text = '{text}'")
        stats = dict(zip(names, stats[0])) if stats else {}
        sigma = str(stats.get("sigma", "--"))
        size = format_size(text_size / 1e9)

        def ratios(sss_sizes):
            return [None if sss_sizes.get(tau) is None
                    else sss_sizes[tau] / (2.0 * text_size / tau)
                    for tau in TAUS]

        constructed = ratios({tau: row[1] for tau, row in measured.items()})
        if any(row[2] for row in measured.values()):
            # The library switched to the periodic variant. The size of the
            # non-periodic set it discarded is reported by helpers/text-stats.
            non_periodic = ratios({tau: stats.get(f"nonperiodic_sss{tau}")
                                   for tau in TAUS})
            rows[text] = [
                (f"{text} (non-periodic variant)", size, sigma, non_periodic, True),
                (f"{text} (periodic variant)", size, sigma, constructed, True)]
        else:
            rows[text] = [(text, size, sigma, constructed, False)]
    return rows


def write_table(rows, out_dir):
    def tex_row(row):
        label, size, sigma, ratios, underlined = row
        label = escape(label)
        cells = [format_ratio(ratio) for ratio in ratios]
        if underlined:
            label = re.sub(r"\((.*)\)", r"(\\underline{\1})", label)
            cells = [f"\\underline{{{cell}}}" for cell in cells]
        return "      & " + " & ".join([label, size, sigma] + cells) + " \\\\"

    tex = ["\\begin{table}",
           "  \\begin{center}",
           "    \\begin{tabular}{c | l c l | c c c c}",
           "      \\toprule",
           "      & & & & \\multicolumn{4}{c}{$\\absolute{S} / (2n/\\tau)$} \\\\",
           "      & data set & size [GB] & $\\sigma$ & "
           + " & ".join(f"$\\tau = {tau}$" for tau in TAUS) + " \\\\"]
    text_table = [("data set", "size [GB]", "sigma")
                  + tuple(f"tau={tau}" for tau in TAUS)]
    for group, parts in TABLE_GROUPS:
        parts = [[row for text in part for row in rows.get(text, [])]
                 for part in parts]
        parts = [part for part in parts if part]
        if not parts:
            continue
        tex.append("      \\midrule")
        count = sum(len(part) for part in parts)
        first = True
        for index, part in enumerate(parts):
            if index:
                tex.append("      \\cmidrule{2-8}")
            for row in part:
                line = tex_row(row)
                if first:
                    line = ("      \\multirow{%d}{*}{\\rotatebox[origin=c]{90}"
                            "{%s}}" % (max(count, 8), group)) + line[6:]
                    first = False
                tex.append(line)
                text_table.append((row[0], row[1], row[2])
                                  + tuple(format_ratio(r) for r in row[3]))
        # keep enough room for the rotated group label
        tex.extend(["      & & & & & & & \\\\"] * max(0, 6 - count))
    tex += ["      \\bottomrule",
            "    \\end{tabular}",
            "  \\end{center}",
            "  \\caption{Additional information about inputs used in evaluation: "
            "name, size $n$, alphabet size $\\sigma$, and the ratios "
            "$\\absolute{S} / (2n / \\tau)$ between the sizes $\\absolute{S}$ of "
            "the synchronizing sets and the expected sizes $2n / \\tau$, for "
            "$\\tau=256$, $512$, $1024$, and $2048$. For texts on which the "
            "periodic variant is constructed, both variants are shown "
            "(underlined).\\label{tab:datasets}}",
            "\\end{table}"]
    with open(os.path.join(out_dir, "table-datasets.tex"), "w") as out:
        out.write("\n".join(tex) + "\n")
    widths = [max(len(row[column]) for row in text_table)
              for column in range(len(text_table[0]))]
    with open(os.path.join(out_dir, "table-datasets.txt"), "w") as out:
        out.write("Table 1: text sizes, alphabet sizes and |S| / (2n/tau)\n\n")
        for row in text_table:
            out.write("  ".join(cell.ljust(width)
                                for cell, width in zip(row, widths)).rstrip() + "\n")


# --------------------------------------------------------------------------

def measured_texts(results_dir):
    texts = []
    for name in ("lce.results.new", "lce.results.new.par"):
        for row in read_results(os.path.join(results_dir, name)):
            if row.get("text") and row["text"] not in texts:
                texts.append(row["text"])
    for row in read_results(os.path.join(results_dir, "pred.results.new")):
        text = re.sub(r"\.sss\d+$", "", row.get("data", ""))
        if text and text not in texts:
            texts.append(text)
    return texts


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("--results", required=True,
                        help="directory with lce.results.new, "
                             "lce.results.new.par and pred.results.new")
    parser.add_argument("--stats", help="text-stats.txt (default: in --results)")
    parser.add_argument("--templates", required=True,
                        help="directory with the paper's plot sources")
    parser.add_argument("--out", required=True, help="output directory")
    args = parser.parse_args()

    plots_dir = os.path.join(args.out, "plots")
    os.makedirs(plots_dir, exist_ok=True)
    for name in sorted(os.listdir(args.templates)):
        if not name.endswith(".tex"):
            continue
        lines = fill_template(os.path.join(args.templates, name), args.results)
        with open(os.path.join(plots_dir, name), "w") as out:
            out.write("\n".join(lines))

    stats = args.stats or os.path.join(args.results, "text-stats.txt")
    write_table(table_rows(args.results, stats), args.out)

    with open(os.path.join(args.out, "info.tex"), "w") as out:
        def define(name, value):
            out.write(f"\\def\\{name}{{\\detokenize{{{value}}}}}\n")
        define("ReproResults", os.path.abspath(args.results))
        define("ReproHost", socket.gethostname())
        define("ReproDate", datetime.datetime.now().strftime("%Y-%m-%d %H:%M"))
        define("ReproTexts", ", ".join(measured_texts(args.results)) or "none")
    if warnings:
        print(f"{len(warnings)} warning(s), see above", file=sys.stderr)


if __name__ == "__main__":
    main()
