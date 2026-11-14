# Conformance

<!-- Written by tests/conformance/run.py --update: don't edit by hand. -->

The language measured against the [cross-dialect Clojure test suite](https://github.com/jank-lang/clojure-test-suite) (MPL-2.0), at commit `a9f32fd7` (its main branch on 2026-11-14). The suite is fetched by its checksum to run, never copied here; how to run it is in [tests/conformance/README.md](../../tests/conformance/README.md), and what differs on purpose in [reference/differences.md](reference/differences.md).

## Totals

| | |
|---|---|
| Files read | 135 of 269 |
| Functions defined | 174 of the 267 the suite tests |
| Assertions passed | 2273 |
| Assertions failed | 62 |
| Assertions that raised an error | 389 |
| Assertions out of scope | 1220 |
| Assertions not run | 3685 |
| Files past their budget | 0 |

Read them so: an assertion **passed** when the language gave the value Clojure gives; it **failed** when it gave another, or **raised an error** (a missing function, a wrong argument, a form the language refuses); it is **out of scope** when it uses something the language leaves out on purpose (counted without running it); it was **not run** when its function isn't defined, or the form around it stopped before it. Errors also count the top-level forms and tests that stopped.

## Out of scope, by reason

| Reason | Assertions |
|---|---|
| [numbers](reference/differences.md#numbers-are-jsons) | 494 |
| [regex](reference/differences.md#no-regular-expressions) | 51 |
| [vars](reference/differences.md#no-metadata-vars-protocols-records-or-multimethods) | 1 |
| [mutation](reference/differences.md#nothing-changes-a-value) | 294 |
| [laziness](reference/differences.md#nothing-is-lazy) | 104 |
| [host](reference/differences.md#no-host-interop-output-clock-or-randomness) | 76 |
| [metadata](reference/differences.md#no-metadata-vars-protocols-records-or-multimethods) | 10 |
| [keys](reference/differences.md#a-maps-keys-are-text) | 104 |
| [namespaces](reference/differences.md#one-flat-space-of-names-per-program) | 12 |
| sets (not merged yet): not a difference, the reader doesn't take `#{...}` yet; counted again once it does | 74 |

## Failing today

The files with assertions that failed or raised an error, most first: the gaps, in the open.

| File | Function | Failed | Errors | Passed |
|---|---|---|---|---|
| `clojure/core_test/case.cljc` | `case` | 0 | 38 | 34 |
| `clojure/core_test/identity.cljc` | `identity` | 0 | 21 | 0 |
| `clojure/core_test/drop_while.cljc` | `drop-while` | 0 | 20 | 0 |
| `clojure/core_test/defn.cljc` | `clojure.core/defn` | 4 | 15 | 10 |
| `clojure/core_test/into.cljc` | `clojure.core/into` | 2 | 14 | 14 |
| `clojure/core_test/contains_qmark.cljc` | `contains?` | 3 | 9 | 27 |
| `clojure/core_test/mod.cljc` | `mod` | 0 | 12 | 49 |
| `clojure/core_test/quot.cljc` | `quot` | 0 | 12 | 37 |
| `clojure/core_test/rem.cljc` | `rem` | 0 | 12 | 37 |
| `clojure/core_test/drop.cljc` | `drop` | 0 | 9 | 11 |
| `clojure/core_test/seq_qmark.cljc` | `seq?` | 3 | 6 | 12 |
| `clojure/core_test/take_nth.cljc` | `take-nth` | 0 | 9 | 6 |
| `clojure/core_test/drop_last.cljc` | `drop-last` | 0 | 8 | 49 |
| `clojure/core_test/take.cljc` | `take` | 0 | 8 | 11 |
| `clojure/core_test/vector_qmark.cljc` | `vector?` | 2 | 6 | 13 |
| `clojure/core_test/coll_qmark.cljc` | `coll?` | 0 | 7 | 18 |
| `clojure/core_test/merge.cljc` | `merge` | 4 | 3 | 21 |
| `clojure/string_test/replace.cljc` | `str/replace` | 2 | 5 | 21 |
| `clojure/string_test/starts_with_qmark.cljc` | `str/starts-with?` | 0 | 7 | 12 |
| `clojure/core_test/apply.cljc` | `apply` | 0 | 6 | 17 |
| `clojure/core_test/keys.cljc` | `keys` | 2 | 4 | 3 |
| `clojure/core_test/list_qmark.cljc` | `list?` | 0 | 6 | 14 |
| `clojure/core_test/map_qmark.cljc` | `map?` | 0 | 6 | 14 |
| `clojure/core_test/subs.cljc` | `subs` | 3 | 3 | 23 |
| `clojure/core_test/repeat.cljc` | `repeat` | 0 | 5 | 8 |
| `clojure/core_test/sort_by.cljc` | `sort-by` | 1 | 4 | 22 |
| `clojure/core_test/update.cljc` | `update` | 0 | 5 | 49 |
| `clojure/core_test/assoc.cljc` | `assoc` | 0 | 4 | 34 |
| `clojure/core_test/ex_info.cljc` | `clojure.core/ex-info` | 0 | 4 | 7 |
| `clojure/core_test/get_in.cljc` | `get-in` | 2 | 2 | 14 |
| `clojure/core_test/nth.cljc` | `nth` | 0 | 4 | 11 |
| `clojure/core_test/star.cljc` | `*` | 2 | 2 | 64 |
| `clojure/core_test/vals.cljc` | `vals` | 2 | 2 | 5 |
| `clojure/string_test/ends_with_qmark.cljc` | `str/ends-with?` | 0 | 4 | 12 |
| `clojure/core_test/dissoc.cljc` | `defrecord` | 0 | 3 | 11 |
| `clojure/core_test/distinct.cljc` | `distinct` | 0 | 3 | 0 |
| `clojure/core_test/gt.cljc` | `>` | 0 | 3 | 40 |
| `clojure/core_test/gt_eq.cljc` | `>=` | 0 | 3 | 46 |
| `clojure/core_test/ifn_qmark.cljc` | `defprotocol` | 2 | 1 | 11 |
| `clojure/core_test/interpose.cljc` | `interpose` | 0 | 3 | 6 |
| `clojure/core_test/lt.cljc` | `<` | 0 | 3 | 40 |
| `clojure/core_test/lt_eq.cljc` | `<=` | 0 | 3 | 45 |
| `clojure/core_test/sort.cljc` | `sort` | 0 | 3 | 13 |
| `clojure/core_test/take_while.cljc` | `take-while` | 0 | 3 | 5 |
| `clojure/edn_test/read_string.cljc` | `clojure.edn/read-string` | 0 | 3 | 0 |
| `clojure/string_test/capitalize.cljc` | `str/capitalize` | 0 | 3 | 7 |
| `clojure/string_test/upper_case.cljc` | `str/upper-case` | 0 | 3 | 7 |
| `clojure/core_test/associative_qmark.cljc` | `clojure.core/associative?` | 2 | 0 | 11 |
| `clojure/core_test/butlast.cljc` | `butlast` | 0 | 2 | 14 |
| `clojure/core_test/conj.cljc` | `conj` | 1 | 1 | 19 |
| `clojure/core_test/eq.cljc` | `=` | 0 | 2 | 0 |
| `clojure/core_test/find.cljc` | `find` | 0 | 2 | 3 |
| `clojure/core_test/first.cljc` | `first` | 0 | 2 | 10 |
| `clojure/core_test/interleave.cljc` | `interleave` | 0 | 2 | 10 |
| `clojure/core_test/mapcat.cljc` | `mapcat` | 0 | 2 | 20 |
| `clojure/core_test/minus.cljc` | `-` | 2 | 0 | 64 |
| `clojure/core_test/next.cljc` | `next` | 0 | 2 | 16 |
| `clojure/core_test/peek.cljc` | `peek` | 2 | 0 | 8 |
| `clojure/core_test/plus.cljc` | `+` | 2 | 0 | 68 |
| `clojure/core_test/portability.cljc` | — | 0 | 2 | 0 |
| `clojure/core_test/rest.cljc` | `rest` | 0 | 2 | 15 |
| `clojure/core_test/second.cljc` | `second` | 0 | 2 | 6 |
| `clojure/core_test/some.cljc` | `some` | 0 | 2 | 21 |
| `clojure/core_test/some_fn.cljc` | `some-fn` | 2 | 0 | 13 |
| `clojure/core_test/subvec.cljc` | `subvec` | 1 | 1 | 24 |
| `clojure/string_test/lower_case.cljc` | `str/lower-case` | 0 | 2 | 7 |
| `clojure/core_test/abs.cljc` | `abs` | 0 | 1 | 0 |
| `clojure/core_test/bit_not.cljc` | `bit-not` | 0 | 1 | 0 |
| `clojure/core_test/bit_or.cljc` | `bit-or` | 0 | 1 | 0 |
| `clojure/core_test/bit_shift_left.cljc` | `bit-shift-left` | 0 | 1 | 0 |
| `clojure/core_test/bit_shift_right.cljc` | `bit-shift-right` | 0 | 1 | 0 |
| `clojure/core_test/bit_xor.cljc` | `bit-xor` | 0 | 1 | 0 |
| `clojure/core_test/boolean.cljc` | `boolean` | 0 | 1 | 0 |
| `clojure/core_test/compare.cljc` | `compare` | 0 | 1 | 0 |
| `clojure/core_test/cond.cljc` | `clojure.core/cond` | 0 | 1 | 0 |
| `clojure/core_test/cons.cljc` | `cons` | 0 | 1 | 0 |
| `clojure/core_test/constantly.cljc` | `constantly` | 0 | 1 | 0 |
| `clojure/core_test/count.cljc` | `count` | 1 | 0 | 16 |
| `clojure/core_test/dec.cljc` | `dec` | 1 | 0 | 8 |
| `clojure/core_test/double.cljc` | `double` | 0 | 1 | 0 |
| `clojure/core_test/double_qmark.cljc` | `double?` | 0 | 1 | 0 |
| `clojure/core_test/empty_qmark.cljc` | `empty?` | 1 | 0 | 10 |
| `clojure/core_test/false_qmark.cljc` | `false?` | 0 | 1 | 0 |
| `clojure/core_test/filter.cljc` | `clojure.core/filter` | 0 | 1 | 0 |
| `clojure/core_test/float_qmark.cljc` | `float?` | 0 | 1 | 0 |
| `clojure/core_test/fn_qmark.cljc` | `fn?` | 0 | 1 | 13 |
| `clojure/core_test/fnext.cljc` | `fnext` | 1 | 0 | 15 |
| `clojure/core_test/group_by.cljc` | `group-by` | 0 | 1 | 2 |
| `clojure/core_test/inc.cljc` | `inc` | 1 | 0 | 9 |
| `clojure/core_test/int.cljc` | `int` | 0 | 1 | 0 |
| `clojure/core_test/int_qmark.cljc` | `int?` | 0 | 1 | 0 |
| `clojure/core_test/integer_qmark.cljc` | `integer?` | 0 | 1 | 0 |
| `clojure/core_test/key.cljc` | `key` | 1 | 0 | 8 |
| `clojure/core_test/keyword.cljc` | `keyword` | 0 | 1 | 0 |
| `clojure/core_test/last.cljc` | `last` | 1 | 0 | 9 |
| `clojure/core_test/map.cljc` | `map` | 0 | 1 | 0 |
| `clojure/core_test/max.cljc` | `max` | 0 | 1 | 14 |
| `clojure/core_test/min.cljc` | `min` | 0 | 1 | 14 |
| `clojure/core_test/minus_gt.cljc` | `clojure.core/->` | 0 | 1 | 17 |
| `clojure/core_test/minus_gt_gt.cljc` | `clojure.core/->>` | 0 | 1 | 29 |
| `clojure/core_test/name.cljc` | `name` | 0 | 1 | 0 |
| `clojure/core_test/neg_int_qmark.cljc` | `neg-int?` | 0 | 1 | 0 |
| `clojure/core_test/nil_qmark.cljc` | `nil?` | 0 | 1 | 0 |
| `clojure/core_test/not_empty.cljc` | `not-empty` | 1 | 0 | 10 |
| `clojure/core_test/not_eq.cljc` | `not=` | 0 | 1 | 0 |
| `clojure/core_test/nthnext.cljc` | `nthnext` | 0 | 1 | 12 |
| `clojure/core_test/nthrest.cljc` | `nthrest` | 1 | 0 | 13 |
| `clojure/core_test/number_qmark.cljc` | `number?` | 0 | 1 | 0 |
| `clojure/core_test/parse_boolean.cljc` | `parse-boolean` | 1 | 0 | 24 |
| `clojure/core_test/parse_double.cljc` | `parse-double` | 1 | 0 | 32 |
| `clojure/core_test/parse_long.cljc` | `parse-long` | 1 | 0 | 29 |
| `clojure/core_test/pos_int_qmark.cljc` | `pos-int?` | 0 | 1 | 0 |
| `clojure/core_test/reduce.cljc` | `reduce` | 0 | 1 | 0 |
| `clojure/core_test/reduce_kv.cljc` | `reduce-kv` | 0 | 1 | 0 |
| `clojure/core_test/remove.cljc` | `remove` | 0 | 1 | 3 |
| `clojure/core_test/reverse.cljc` | `reverse` | 1 | 0 | 10 |
| `clojure/core_test/select_keys.cljc` | `select-keys` | 0 | 1 | 6 |
| `clojure/core_test/seq.cljc` | `seq` | 0 | 1 | 6 |
| `clojure/core_test/str.cljc` | `str` | 0 | 1 | 0 |
| `clojure/core_test/string_qmark.cljc` | `string?` | 0 | 1 | 0 |
| `clojure/core_test/symbol.cljc` | `symbol` | 0 | 1 | 0 |
| `clojure/core_test/true_qmark.cljc` | `true?` | 0 | 1 | 0 |
| `clojure/core_test/val.cljc` | `val` | 1 | 0 | 8 |
| `clojure/core_test/vec.cljc` | `vec` | 0 | 1 | 10 |
| `clojure/string_test/blank_qmark.cljc` | `str/blank?` | 1 | 0 | 12 |
| `clojure/string_test/split_lines.cljc` | `str/split-lines` | 1 | 0 | 18 |

## Every file

| File | Function | Defined | Passed | Failed | Errors | Out of scope |
|---|---|---|---|---|---|---|
| `clojure/core_test/abs.cljc` | `abs` | doesn't read | 0 | 0 | 1 | 0 |
| `clojure/core_test/aclone.cljc` | `aclone` | doesn't read | 0 | 0 | 0 | 0 |
| `clojure/core_test/add_watch.cljc` | `add-watch` | no | 0 | 0 | 0 | 0 |
| `clojure/core_test/ancestors.cljc` | `ancestors` | doesn't read | 0 | 0 | 0 | 0 |
| `clojure/core_test/and.cljc` | `and` | yes | 13 | 0 | 0 | 7 |
| `clojure/core_test/any_qmark.cljc` | `any?` | no | 0 | 0 | 0 | 0 |
| `clojure/core_test/apply.cljc` | `apply` | doesn't read | 17 | 0 | 6 | 7 |
| `clojure/core_test/assoc.cljc` | `assoc` | yes | 34 | 0 | 4 | 2 |
| `clojure/core_test/assoc_bang.cljc` | `assoc!` | no | 0 | 0 | 0 | 0 |
| `clojure/core_test/associative_qmark.cljc` | `clojure.core/associative?` | doesn't read | 11 | 2 | 0 | 4 |
| `clojure/core_test/atom.cljc` | `clojure.core/atom` | doesn't read | 0 | 0 | 0 | 0 |
| `clojure/core_test/bigdec.cljc` | `bigdec` | no | 0 | 0 | 0 | 0 |
| `clojure/core_test/bigint.cljc` | `bigint` | doesn't read | 0 | 0 | 0 | 0 |
| `clojure/core_test/binding.cljc` | `binding` | doesn't read | 0 | 0 | 0 | 0 |
| `clojure/core_test/bit_and.cljc` | `bit-and` | yes | 11 | 0 | 0 | 0 |
| `clojure/core_test/bit_and_not.cljc` | `bit-and-not` | no | 0 | 0 | 0 | 0 |
| `clojure/core_test/bit_clear.cljc` | `bit-clear` | no | 0 | 0 | 0 | 0 |
| `clojure/core_test/bit_flip.cljc` | `bit-flip` | doesn't read | 0 | 0 | 0 | 0 |
| `clojure/core_test/bit_not.cljc` | `bit-not` | doesn't read | 0 | 0 | 1 | 0 |
| `clojure/core_test/bit_or.cljc` | `bit-or` | doesn't read | 0 | 0 | 1 | 0 |
| `clojure/core_test/bit_set.cljc` | `bit-set` | doesn't read | 0 | 0 | 0 | 0 |
| `clojure/core_test/bit_shift_left.cljc` | `bit-shift-left` | doesn't read | 0 | 0 | 1 | 0 |
| `clojure/core_test/bit_shift_right.cljc` | `bit-shift-right` | doesn't read | 0 | 0 | 1 | 0 |
| `clojure/core_test/bit_test.cljc` | `bit-test` | doesn't read | 0 | 0 | 0 | 0 |
| `clojure/core_test/bit_xor.cljc` | `bit-xor` | doesn't read | 0 | 0 | 1 | 0 |
| `clojure/core_test/boolean.cljc` | `boolean` | doesn't read | 0 | 0 | 1 | 0 |
| `clojure/core_test/boolean_qmark.cljc` | `boolean?` | doesn't read | 17 | 0 | 0 | 1 |
| `clojure/core_test/bound_fn.cljc` | `bound-fn` | doesn't read | 0 | 0 | 0 | 0 |
| `clojure/core_test/bound_fn_star.cljc` | `bound-fn*` | doesn't read | 0 | 0 | 0 | 0 |
| `clojure/core_test/butlast.cljc` | `butlast` | doesn't read | 14 | 0 | 2 | 2 |
| `clojure/core_test/byte.cljc` | `byte` | doesn't read | 0 | 0 | 0 | 0 |
| `clojure/core_test/case.cljc` | `case` | doesn't read | 34 | 0 | 38 | 5 |
| `clojure/core_test/char.cljc` | `char` | no | 0 | 0 | 0 | 0 |
| `clojure/core_test/char_qmark.cljc` | `char?` | doesn't read | 0 | 0 | 0 | 0 |
| `clojure/core_test/coll_qmark.cljc` | `coll?` | doesn't read | 18 | 0 | 7 | 3 |
| `clojure/core_test/comment.cljc` | `comment` | yes | 4 | 0 | 0 | 0 |
| `clojure/core_test/compare.cljc` | `compare` | doesn't read | 0 | 0 | 1 | 0 |
| `clojure/core_test/concat.cljc` | `concat` | yes | 3 | 0 | 0 | 17 |
| `clojure/core_test/cond.cljc` | `clojure.core/cond` | doesn't read | 0 | 0 | 1 | 0 |
| `clojure/core_test/cond_minus_gt.cljc` | `clojure.core/cond->` | yes | 30 | 0 | 0 | 28 |
| `clojure/core_test/conj.cljc` | `conj` | doesn't read | 19 | 1 | 1 | 4 |
| `clojure/core_test/conj_bang.cljc` | `conj!` | doesn't read | 0 | 0 | 0 | 0 |
| `clojure/core_test/cons.cljc` | `cons` | doesn't read | 0 | 0 | 1 | 0 |
| `clojure/core_test/constantly.cljc` | `constantly` | doesn't read | 0 | 0 | 1 | 0 |
| `clojure/core_test/contains_qmark.cljc` | `contains?` | doesn't read | 27 | 3 | 9 | 28 |
| `clojure/core_test/count.cljc` | `count` | doesn't read | 16 | 1 | 0 | 3 |
| `clojure/core_test/counted_qmark.cljc` | `counted?` | no | 0 | 0 | 0 | 0 |
| `clojure/core_test/cycle.cljc` | `cycle` | no | 0 | 0 | 0 | 0 |
| `clojure/core_test/dec.cljc` | `dec` | yes | 8 | 1 | 0 | 5 |
| `clojure/core_test/decimal_qmark.cljc` | `decimal?` | doesn't read | 0 | 0 | 0 | 0 |
| `clojure/core_test/defmethod.cljc` | `clojure.core/defmethod` | no | 0 | 0 | 0 | 0 |
| `clojure/core_test/defn.cljc` | `clojure.core/defn` | yes | 10 | 4 | 15 | 7 |
| `clojure/core_test/denominator.cljc` | `denominator` | no | 0 | 0 | 0 | 0 |
| `clojure/core_test/derive.cljc` | `derive` | doesn't read | 0 | 0 | 0 | 0 |
| `clojure/core_test/descendants.cljc` | `descendants` | doesn't read | 0 | 0 | 0 | 0 |
| `clojure/core_test/destructure.cljc` | `destructure` | no | 0 | 0 | 0 | 0 |
| `clojure/core_test/disj.cljc` | `disj` | doesn't read | 0 | 0 | 0 | 0 |
| `clojure/core_test/disj_bang.cljc` | `disj!` | doesn't read | 0 | 0 | 0 | 0 |
| `clojure/core_test/dissoc.cljc` | `defrecord` | doesn't read | 11 | 0 | 3 | 3 |
| `clojure/core_test/dissoc_bang.cljc` | `dissoc!` | doesn't read | 0 | 0 | 0 | 0 |
| `clojure/core_test/distinct.cljc` | `distinct` | doesn't read | 0 | 0 | 3 | 0 |
| `clojure/core_test/doseq.cljc` | `doseq` | yes | 1 | 0 | 0 | 14 |
| `clojure/core_test/double.cljc` | `double` | doesn't read | 0 | 0 | 1 | 0 |
| `clojure/core_test/double_qmark.cljc` | `double?` | doesn't read | 0 | 0 | 1 | 0 |
| `clojure/core_test/drop.cljc` | `drop` | doesn't read | 11 | 0 | 9 | 2 |
| `clojure/core_test/drop_last.cljc` | `drop-last` | doesn't read | 49 | 0 | 8 | 14 |
| `clojure/core_test/drop_while.cljc` | `drop-while` | doesn't read | 0 | 0 | 20 | 4 |
| `clojure/core_test/empty.cljc` | `empty` | doesn't read | 0 | 0 | 0 | 0 |
| `clojure/core_test/empty_qmark.cljc` | `empty?` | doesn't read | 10 | 1 | 0 | 4 |
| `clojure/core_test/eq.cljc` | `=` | doesn't read | 0 | 0 | 2 | 0 |
| `clojure/core_test/eval.cljc` | `eval` | doesn't read | 0 | 0 | 0 | 0 |
| `clojure/core_test/even_qmark.cljc` | `even?` | yes | 13 | 0 | 0 | 4 |
| `clojure/core_test/every_qmark.cljc` | `every?` | doesn't read | 22 | 0 | 0 | 9 |
| `clojure/core_test/ex_info.cljc` | `clojure.core/ex-info` | yes | 7 | 0 | 4 | 2 |
| `clojure/core_test/false_qmark.cljc` | `false?` | doesn't read | 0 | 0 | 1 | 0 |
| `clojure/core_test/ffirst.cljc` | `ffirst` | doesn't read | 12 | 0 | 0 | 5 |
| `clojure/core_test/filter.cljc` | `clojure.core/filter` | yes | 0 | 0 | 1 | 0 |
| `clojure/core_test/find.cljc` | `find` | yes | 3 | 0 | 2 | 2 |
| `clojure/core_test/first.cljc` | `first` | doesn't read | 10 | 0 | 2 | 5 |
| `clojure/core_test/float.cljc` | `float` | doesn't read | 0 | 0 | 0 | 0 |
| `clojure/core_test/float_qmark.cljc` | `float?` | doesn't read | 0 | 0 | 1 | 0 |
| `clojure/core_test/fn.cljc` | `clojure.core/fn` | yes | 16 | 0 | 0 | 42 |
| `clojure/core_test/fn_qmark.cljc` | `fn?` | doesn't read | 13 | 0 | 1 | 1 |
| `clojure/core_test/fnext.cljc` | `fnext` | doesn't read | 15 | 1 | 0 | 3 |
| `clojure/core_test/fnil.cljc` | `fnil` | yes | 18 | 0 | 0 | 0 |
| `clojure/core_test/format.cljc` | `format` | yes | 1 | 0 | 0 | 0 |
| `clojure/core_test/get.cljc` | `get` | doesn't read | 0 | 0 | 0 | 31 |
| `clojure/core_test/get_in.cljc` | `get-in` | yes | 14 | 2 | 2 | 4 |
| `clojure/core_test/group_by.cljc` | `group-by` | yes | 2 | 0 | 1 | 2 |
| `clojure/core_test/gt.cljc` | `>` | yes | 40 | 0 | 3 | 21 |
| `clojure/core_test/gt_eq.cljc` | `>=` | yes | 46 | 0 | 3 | 23 |
| `clojure/core_test/hash_map.cljc` | `hash-map` | yes | 7 | 0 | 0 | 1 |
| `clojure/core_test/hash_set.cljc` | `hash-set` | doesn't read | 0 | 0 | 0 | 0 |
| `clojure/core_test/ident_qmark.cljc` | `ident?` | yes | 11 | 0 | 0 | 2 |
| `clojure/core_test/identical_qmark.cljc` | `identical?` | no | 0 | 0 | 0 | 0 |
| `clojure/core_test/identity.cljc` | `identity` | yes | 0 | 0 | 21 | 6 |
| `clojure/core_test/if_let.cljc` | `clojure.core/if-let` | yes | 10 | 0 | 0 | 36 |
| `clojure/core_test/ifn_qmark.cljc` | `defprotocol` | doesn't read | 11 | 2 | 1 | 2 |
| `clojure/core_test/inc.cljc` | `inc` | yes | 9 | 1 | 0 | 5 |
| `clojure/core_test/int.cljc` | `int` | doesn't read | 0 | 0 | 1 | 0 |
| `clojure/core_test/int_qmark.cljc` | `int?` | doesn't read | 0 | 0 | 1 | 0 |
| `clojure/core_test/integer_qmark.cljc` | `integer?` | doesn't read | 0 | 0 | 1 | 0 |
| `clojure/core_test/interleave.cljc` | `interleave` | doesn't read | 10 | 0 | 2 | 4 |
| `clojure/core_test/intern.cljc` | `intern` | no | 0 | 0 | 0 | 0 |
| `clojure/core_test/interpose.cljc` | `interpose` | doesn't read | 6 | 0 | 3 | 1 |
| `clojure/core_test/into.cljc` | `clojure.core/into` | doesn't read | 14 | 2 | 14 | 2 |
| `clojure/core_test/juxt.cljc` | `juxt` | yes | 10 | 0 | 0 | 3 |
| `clojure/core_test/key.cljc` | `key` | doesn't read | 8 | 1 | 0 | 4 |
| `clojure/core_test/keys.cljc` | `keys` | doesn't read | 3 | 2 | 4 | 4 |
| `clojure/core_test/keyword.cljc` | `keyword` | doesn't read | 0 | 0 | 1 | 0 |
| `clojure/core_test/keyword_qmark.cljc` | `keyword?` | yes | 11 | 0 | 0 | 2 |
| `clojure/core_test/last.cljc` | `last` | doesn't read | 9 | 1 | 0 | 1 |
| `clojure/core_test/lazy_seq.cljc` | `lazy-seq` | no | 0 | 0 | 0 | 0 |
| `clojure/core_test/let.cljc` | `let` | yes | 7 | 0 | 0 | 38 |
| `clojure/core_test/list.cljc` | `list` | doesn't read | 29 | 0 | 0 | 3 |
| `clojure/core_test/list_qmark.cljc` | `list?` | yes | 14 | 0 | 6 | 2 |
| `clojure/core_test/long.cljc` | `long` | doesn't read | 0 | 0 | 0 | 0 |
| `clojure/core_test/lt.cljc` | `<` | yes | 40 | 0 | 3 | 21 |
| `clojure/core_test/lt_eq.cljc` | `<=` | yes | 45 | 0 | 3 | 23 |
| `clojure/core_test/make_hierarchy.cljc` | `make-hierarchy` | no | 0 | 0 | 0 | 0 |
| `clojure/core_test/map.cljc` | `map` | yes | 0 | 0 | 1 | 0 |
| `clojure/core_test/map_qmark.cljc` | `map?` | yes | 14 | 0 | 6 | 2 |
| `clojure/core_test/mapcat.cljc` | `mapcat` | yes | 20 | 0 | 2 | 2 |
| `clojure/core_test/mapv.cljc` | `clojure.core/mapv` | yes | 5 | 0 | 0 | 56 |
| `clojure/core_test/max.cljc` | `max` | yes | 14 | 0 | 1 | 11 |
| `clojure/core_test/merge.cljc` | `merge` | yes | 21 | 4 | 3 | 2 |
| `clojure/core_test/min.cljc` | `min` | yes | 14 | 0 | 1 | 11 |
| `clojure/core_test/min_key.cljc` | `min-key` | yes | 30 | 0 | 0 | 19 |
| `clojure/core_test/minus.cljc` | `-` | yes | 64 | 2 | 0 | 73 |
| `clojure/core_test/minus_gt.cljc` | `clojure.core/->` | yes | 17 | 0 | 1 | 16 |
| `clojure/core_test/minus_gt_gt.cljc` | `clojure.core/->>` | yes | 29 | 0 | 1 | 25 |
| `clojure/core_test/mod.cljc` | `mod` | yes | 49 | 0 | 12 | 23 |
| `clojure/core_test/name.cljc` | `name` | doesn't read | 0 | 0 | 1 | 0 |
| `clojure/core_test/namespace.cljc` | `namespace` | no | 0 | 0 | 0 | 0 |
| `clojure/core_test/nan_qmark.cljc` | `NaN?` | no | 0 | 0 | 0 | 0 |
| `clojure/core_test/neg_int_qmark.cljc` | `neg-int?` | doesn't read | 0 | 0 | 1 | 0 |
| `clojure/core_test/neg_qmark.cljc` | `neg?` | yes | 20 | 0 | 0 | 5 |
| `clojure/core_test/next.cljc` | `next` | doesn't read | 16 | 0 | 2 | 5 |
| `clojure/core_test/nfirst.cljc` | `nfirst` | doesn't read | 13 | 0 | 0 | 4 |
| `clojure/core_test/nil_qmark.cljc` | `nil?` | doesn't read | 0 | 0 | 1 | 0 |
| `clojure/core_test/nnext.cljc` | `nnext` | doesn't read | 6 | 0 | 0 | 2 |
| `clojure/core_test/not.cljc` | `not` | yes | 4 | 0 | 0 | 1 |
| `clojure/core_test/not_empty.cljc` | `not-empty` | doesn't read | 10 | 1 | 0 | 2 |
| `clojure/core_test/not_eq.cljc` | `not=` | yes | 0 | 0 | 1 | 0 |
| `clojure/core_test/not_every_qmark.cljc` | `not-every?` | yes | 22 | 0 | 0 | 9 |
| `clojure/core_test/nth.cljc` | `nth` | doesn't read | 11 | 0 | 4 | 4 |
| `clojure/core_test/nthnext.cljc` | `nthnext` | yes | 12 | 0 | 1 | 0 |
| `clojure/core_test/nthrest.cljc` | `nthrest` | yes | 13 | 1 | 0 | 0 |
| `clojure/core_test/num.cljc` | `num` | doesn't read | 0 | 0 | 0 | 0 |
| `clojure/core_test/number_qmark.cljc` | `number?` | doesn't read | 0 | 0 | 1 | 0 |
| `clojure/core_test/number_range.cljc` | — | — | 0 | 0 | 0 | 0 |
| `clojure/core_test/numerator.cljc` | `numerator` | no | 0 | 0 | 0 | 0 |
| `clojure/core_test/odd_qmark.cljc` | `odd?` | yes | 13 | 0 | 0 | 4 |
| `clojure/core_test/or.cljc` | `or` | yes | 13 | 0 | 0 | 7 |
| `clojure/core_test/parents.cljc` | `parents` | doesn't read | 0 | 0 | 0 | 0 |
| `clojure/core_test/parse_boolean.cljc` | `parse-boolean` | doesn't read | 24 | 1 | 0 | 1 |
| `clojure/core_test/parse_double.cljc` | `parse-double` | doesn't read | 32 | 1 | 0 | 3 |
| `clojure/core_test/parse_long.cljc` | `parse-long` | doesn't read | 29 | 1 | 0 | 1 |
| `clojure/core_test/parse_uuid.cljc` | `parse-uuid` | doesn't read | 0 | 0 | 0 | 0 |
| `clojure/core_test/partial.cljc` | `partial` | yes | 4 | 0 | 0 | 5 |
| `clojure/core_test/partition.cljc` | `partition` | yes | 16 | 0 | 0 | 4 |
| `clojure/core_test/peek.cljc` | `peek` | yes | 8 | 2 | 0 | 1 |
| `clojure/core_test/persistent_bang.cljc` | `persistent!` | doesn't read | 0 | 0 | 0 | 0 |
| `clojure/core_test/plus.cljc` | `+` | yes | 68 | 2 | 0 | 59 |
| `clojure/core_test/plus_squote.cljc` | `+'` | doesn't read | 0 | 0 | 0 | 0 |
| `clojure/core_test/pop.cljc` | `pop` | yes | 10 | 0 | 0 | 2 |
| `clojure/core_test/pop_bang.cljc` | `pop!` | no | 0 | 0 | 0 | 0 |
| `clojure/core_test/portability.cljc` | — | doesn't read | 0 | 0 | 2 | 0 |
| `clojure/core_test/pos_int_qmark.cljc` | `pos-int?` | doesn't read | 0 | 0 | 1 | 0 |
| `clojure/core_test/pos_qmark.cljc` | `pos?` | yes | 20 | 0 | 0 | 5 |
| `clojure/core_test/pr_str.cljc` | `pr-str` | doesn't read | 1 | 0 | 0 | 1 |
| `clojure/core_test/print_str.cljc` | `print-str` | doesn't read | 0 | 0 | 0 | 0 |
| `clojure/core_test/println.cljc` | `clojure.core/println` | doesn't read | 0 | 0 | 0 | 0 |
| `clojure/core_test/println_str.cljc` | `println-str` | doesn't read | 0 | 0 | 0 | 0 |
| `clojure/core_test/prn_str.cljc` | `prn-str` | doesn't read | 0 | 0 | 0 | 0 |
| `clojure/core_test/qualified_ident_qmark.cljc` | `qualified-ident?` | no | 0 | 0 | 0 | 0 |
| `clojure/core_test/qualified_keyword_qmark.cljc` | `qualified-keyword?` | no | 0 | 0 | 0 | 0 |
| `clojure/core_test/qualified_symbol_qmark.cljc` | `qualified-symbol?` | no | 0 | 0 | 0 | 0 |
| `clojure/core_test/quot.cljc` | `quot` | yes | 37 | 0 | 12 | 20 |
| `clojure/core_test/rand.cljc` | `rand` | no | 0 | 0 | 0 | 0 |
| `clojure/core_test/rand_int.cljc` | `rand-int` | no | 0 | 0 | 0 | 0 |
| `clojure/core_test/rand_nth.cljc` | `rand-nth` | no | 0 | 0 | 0 | 0 |
| `clojure/core_test/random_sample.cljc` | `random-sample` | no | 0 | 0 | 0 | 0 |
| `clojure/core_test/random_uuid.cljc` | `random-uuid` | no | 0 | 0 | 0 | 0 |
| `clojure/core_test/ratio_qmark.cljc` | `ratio?` | doesn't read | 0 | 0 | 0 | 0 |
| `clojure/core_test/rational_qmark.cljc` | `rational?` | doesn't read | 0 | 0 | 0 | 0 |
| `clojure/core_test/rationalize.cljc` | `rationalize` | doesn't read | 0 | 0 | 0 | 0 |
| `clojure/core_test/realized_qmark.cljc` | `realized?` | no | 0 | 0 | 0 | 0 |
| `clojure/core_test/reduce.cljc` | `reduce` | doesn't read | 0 | 0 | 1 | 0 |
| `clojure/core_test/reduce_kv.cljc` | `reduce-kv` | doesn't read | 0 | 0 | 1 | 0 |
| `clojure/core_test/rem.cljc` | `rem` | yes | 37 | 0 | 12 | 19 |
| `clojure/core_test/remove.cljc` | `remove` | doesn't read | 3 | 0 | 1 | 14 |
| `clojure/core_test/remove_watch.cljc` | `remove-watch` | no | 0 | 0 | 0 | 0 |
| `clojure/core_test/repeat.cljc` | `repeat` | yes | 8 | 0 | 5 | 4 |
| `clojure/core_test/repeatedly.cljc` | `repeatedly` | doesn't read | 0 | 0 | 0 | 0 |
| `clojure/core_test/reset_bang.cljc` | `clojure.core/reset!` | doesn't read | 0 | 0 | 0 | 0 |
| `clojure/core_test/rest.cljc` | `rest` | doesn't read | 15 | 0 | 2 | 4 |
| `clojure/core_test/reverse.cljc` | `reverse` | yes | 10 | 1 | 0 | 0 |
| `clojure/core_test/reversible_qmark.cljc` | `reversible?` | no | 0 | 0 | 0 | 0 |
| `clojure/core_test/rseq.cljc` | `rseq` | yes | 12 | 0 | 0 | 0 |
| `clojure/core_test/run_bang.cljc` | `run!` | yes | 4 | 0 | 0 | 12 |
| `clojure/core_test/second.cljc` | `second` | yes | 6 | 0 | 2 | 1 |
| `clojure/core_test/select_keys.cljc` | `select-keys` | doesn't read | 6 | 0 | 1 | 3 |
| `clojure/core_test/seq.cljc` | `seq` | doesn't read | 6 | 0 | 1 | 10 |
| `clojure/core_test/seq_qmark.cljc` | `seq?` | yes | 12 | 3 | 6 | 2 |
| `clojure/core_test/seqable_qmark.cljc` | `seqable?` | no | 0 | 0 | 0 | 0 |
| `clojure/core_test/sequential_qmark.cljc` | `sequential?` | doesn't read | 13 | 0 | 0 | 4 |
| `clojure/core_test/set.cljc` | `set` | doesn't read | 0 | 0 | 0 | 0 |
| `clojure/core_test/set_qmark.cljc` | `set?` | doesn't read | 0 | 0 | 0 | 0 |
| `clojure/core_test/short.cljc` | `short` | doesn't read | 0 | 0 | 0 | 0 |
| `clojure/core_test/shuffle.cljc` | `shuffle` | doesn't read | 0 | 0 | 0 | 0 |
| `clojure/core_test/simple_ident_qmark.cljc` | `simple-ident?` | no | 0 | 0 | 0 | 0 |
| `clojure/core_test/simple_keyword_qmark.cljc` | `simple-keyword?` | no | 0 | 0 | 0 | 0 |
| `clojure/core_test/simple_symbol_qmark.cljc` | `simple-symbol?` | no | 0 | 0 | 0 | 0 |
| `clojure/core_test/slash.cljc` | `/` | yes | 96 | 0 | 0 | 64 |
| `clojure/core_test/some.cljc` | `some` | doesn't read | 21 | 0 | 2 | 15 |
| `clojure/core_test/some_fn.cljc` | `some-fn` | yes | 13 | 2 | 0 | 1 |
| `clojure/core_test/some_qmark.cljc` | `some?` | yes | 4 | 0 | 0 | 1 |
| `clojure/core_test/sort.cljc` | `sort` | doesn't read | 13 | 0 | 3 | 2 |
| `clojure/core_test/sort_by.cljc` | `sort-by` | yes | 22 | 1 | 4 | 1 |
| `clojure/core_test/sorted_qmark.cljc` | `sorted?` | no | 0 | 0 | 0 | 0 |
| `clojure/core_test/special_symbol_qmark.cljc` | `special-symbol?` | no | 0 | 0 | 0 | 0 |
| `clojure/core_test/star.cljc` | `*` | yes | 64 | 2 | 2 | 57 |
| `clojure/core_test/star_squote.cljc` | `*'` | doesn't read | 0 | 0 | 0 | 0 |
| `clojure/core_test/str.cljc` | `str` | doesn't read | 0 | 0 | 1 | 0 |
| `clojure/core_test/string_qmark.cljc` | `string?` | doesn't read | 0 | 0 | 1 | 0 |
| `clojure/core_test/subs.cljc` | `subs` | yes | 23 | 3 | 3 | 0 |
| `clojure/core_test/subvec.cljc` | `subvec` | yes | 24 | 1 | 1 | 8 |
| `clojure/core_test/swap_bang.cljc` | `clojure.core/swap!` | no | 0 | 0 | 0 | 0 |
| `clojure/core_test/symbol.cljc` | `symbol` | doesn't read | 0 | 0 | 1 | 0 |
| `clojure/core_test/symbol_qmark.cljc` | `symbol?` | yes | 11 | 0 | 0 | 2 |
| `clojure/core_test/take.cljc` | `take` | yes | 11 | 0 | 8 | 4 |
| `clojure/core_test/take_last.cljc` | `take-last` | yes | 9 | 0 | 0 | 0 |
| `clojure/core_test/take_nth.cljc` | `take-nth` | yes | 6 | 0 | 9 | 0 |
| `clojure/core_test/take_while.cljc` | `take-while` | yes | 5 | 0 | 3 | 1 |
| `clojure/core_test/taps.cljc` | `add-tap` | no | 0 | 0 | 0 | 0 |
| `clojure/core_test/transient.cljc` | `transient` | no | 0 | 0 | 0 | 0 |
| `clojure/core_test/true_qmark.cljc` | `true?` | doesn't read | 0 | 0 | 1 | 0 |
| `clojure/core_test/underive.cljc` | `underive` | doesn't read | 0 | 0 | 0 | 0 |
| `clojure/core_test/unsigned_bit_shift_right.cljc` | `unsigned-bit-shift-right` | no | 0 | 0 | 0 | 0 |
| `clojure/core_test/update.cljc` | `update` | doesn't read | 49 | 0 | 5 | 9 |
| `clojure/core_test/uuid_qmark.cljc` | `uuid?` | doesn't read | 0 | 0 | 0 | 0 |
| `clojure/core_test/val.cljc` | `val` | doesn't read | 8 | 1 | 0 | 4 |
| `clojure/core_test/vals.cljc` | `vals` | doesn't read | 5 | 2 | 2 | 3 |
| `clojure/core_test/var_qmark.cljc` | `defmulti` | doesn't read | 0 | 0 | 0 | 0 |
| `clojure/core_test/vec.cljc` | `vec` | doesn't read | 10 | 0 | 1 | 9 |
| `clojure/core_test/vector.cljc` | `vector` | doesn't read | 24 | 0 | 0 | 3 |
| `clojure/core_test/vector_qmark.cljc` | `vector?` | yes | 13 | 2 | 6 | 2 |
| `clojure/core_test/when.cljc` | `when` | doesn't read | 17 | 0 | 0 | 3 |
| `clojure/core_test/when_first.cljc` | `when-first` | yes | 5 | 0 | 0 | 8 |
| `clojure/core_test/when_let.cljc` | `when-let` | yes | 5 | 0 | 0 | 8 |
| `clojure/core_test/when_not.cljc` | `when-not` | doesn't read | 16 | 0 | 0 | 3 |
| `clojure/core_test/with_meta.cljc` | `with-meta` | doesn't read | 0 | 0 | 0 | 0 |
| `clojure/core_test/with_out_str.cljc` | `with-out-str` | doesn't read | 0 | 0 | 0 | 0 |
| `clojure/core_test/with_precision.cljc` | `with-precision` | no | 0 | 0 | 0 | 0 |
| `clojure/core_test/zero_qmark.cljc` | `zero?` | yes | 21 | 0 | 0 | 5 |
| `clojure/core_test/zipmap.cljc` | `zipmap` | doesn't read | 4 | 0 | 0 | 12 |
| `clojure/edn_test/read_string.cljc` | `clojure.edn/read-string` | doesn't read | 0 | 0 | 3 | 0 |
| `clojure/string_test/blank_qmark.cljc` | `str/blank?` | yes | 12 | 1 | 0 | 0 |
| `clojure/string_test/capitalize.cljc` | `str/capitalize` | yes | 7 | 0 | 3 | 1 |
| `clojure/string_test/ends_with_qmark.cljc` | `str/ends-with?` | yes | 12 | 0 | 4 | 0 |
| `clojure/string_test/escape.cljc` | `str/escape` | no | 0 | 0 | 0 | 0 |
| `clojure/string_test/lower_case.cljc` | `str/lower-case` | yes | 7 | 0 | 2 | 2 |
| `clojure/string_test/replace.cljc` | `str/replace` | yes | 21 | 2 | 5 | 12 |
| `clojure/string_test/reverse.cljc` | `str/reverse` | yes | 13 | 0 | 0 | 0 |
| `clojure/string_test/split.cljc` | `str/split` | yes | 0 | 0 | 0 | 40 |
| `clojure/string_test/split_lines.cljc` | `str/split-lines` | yes | 18 | 1 | 0 | 0 |
| `clojure/string_test/starts_with_qmark.cljc` | `str/starts-with?` | yes | 12 | 0 | 7 | 3 |
| `clojure/string_test/upper_case.cljc` | `str/upper-case` | yes | 7 | 0 | 3 | 1 |
