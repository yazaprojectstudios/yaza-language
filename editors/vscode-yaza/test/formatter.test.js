"use strict";

const assert = require("assert");
const { formatYaza } = require("../formatter");

const classifier = [
  "fn classify(value: Number) -> Text:",
  "  if value < -10:",
  "        return \"very negative\"",
  "  if value < 0:",
  "        return \"negative\"",
  "  if value == 0:",
  "        return \"zero\"",
  "  if value <= 10:",
  "        return \"positive\"",
  "  return \"very positive\""
].join("\n");

const expectedClassifier = [
  "fn classify(value: Number) -> Text:",
  "    if value < -10:",
  "        return \"very negative\"",
  "    if value < 0:",
  "        return \"negative\"",
  "    if value == 0:",
  "        return \"zero\"",
  "    if value <= 10:",
  "        return \"positive\"",
  "    return \"very positive\"",
  ""
].join("\n");

assert.strictEqual(formatYaza(classifier), expectedClassifier);
assert.strictEqual(formatYaza(expectedClassifier), expectedClassifier, "formatting must be idempotent");

const variantMatch = [
  "fn describe(value: Result[Text, Text]) -> Text:",
  "    match value:",
  "        ok(text):",
  "            return text",
  "        err(_):",
  "            return \"failed\"",
  "    return \"unreachable\""
].join("\n");
assert.strictEqual(formatYaza(variantMatch), variantMatch + "\n");

const quotedComment = "start:\n    say \"a # inside text\" # trailing note\n";
assert.strictEqual(formatYaza(quotedComment), quotedComment);

const crlf = "start:\r\n  say \"ready\"\r\n";
assert.strictEqual(formatYaza(crlf, { eol: "\r\n" }), "start:\r\n    say \"ready\"\r\n");

process.stdout.write("YAZA formatter: indentation, match arms, comments, CRLF, and idempotence passed.\n");
