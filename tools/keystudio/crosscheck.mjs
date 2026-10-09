// Checks that Apex Key Studio (the browser tool) and apex_keygen make the same
// keys: the same licence gives the same key byte for byte, keys from each are
// accepted by the other, and changed keys are rejected.
//   node crosscheck.mjs <apex_keygen> <apex-key-studio.html> <development-secret-key.txt>
// Exits 77 (skipped) when this Node has no Ed25519 in WebCrypto.
import { readFileSync, writeFileSync, mkdtempSync, rmSync } from "node:fs";
import { execFileSync } from "node:child_process";
import { tmpdir } from "node:os";
import { join } from "node:path";

const [keygen, htmlPath, devSecretPath] = process.argv.slice(2);
const html = readFileSync(htmlPath, "utf8");
const core = html.split("/* core:start */")[1].split("/* core:end */")[0];
const ApexLicence = new Function(core + "; return ApexLicence;")();
if (!(await ApexLicence.supported())) { console.log("SKIP: no Ed25519 in this Node"); process.exit(77); }

const run = (...args) => execFileSync(keygen, args).toString().trim();
const devSecret = readFileSync(devSecretPath, "utf8").trim();
const devPublic = devSecret.slice(64);
const tmp = mkdtempSync(join(tmpdir(), "apex-keystudio-"));
let failures = 0;
const check = (ok, what) => { if (!ok) { console.log("FAIL:", what); failures++; } };

try {
  const day = ApexLicence.dayNumber();
  const dev = await ApexLicence.importSecret(devSecret);
  const jsKey = await ApexLicence.sign(dev, { products: 1, issueDay: day, serial: 42, owner: "Cross Check" });
  check(jsKey === run("issue", devSecretPath, "--owner", "Cross Check", "--products", "amp", "--serial", "42"),
        "the browser and apex_keygen make different keys for the same licence");

  const kp = await ApexLicence.generateKeyPair();
  const secretFile = join(tmp, "fresh.key");
  writeFileSync(secretFile, kp.secretHex + "\n");
  check(kp.secretHex.length === 128 && kp.secretHex.endsWith(kp.publicHex), "secret key format");
  const fresh = await ApexLicence.importSecret(kp.secretHex);
  const owner = "Ångström Ölander <test@example.com>";
  const k1 = await ApexLicence.sign(fresh, { products: 255, issueDay: day, serial: 7, owner });
  check(run("check", k1, "--public", kp.publicHex).includes("serial 7"), "apex_keygen rejects a key made in the browser");
  const v2 = await ApexLicence.verify(run("issue", secretFile, "--owner", owner, "--products", "drop", "--serial", "99"), kp.publicHex);
  check(v2 && v2.owner === owner && v2.products === 2 && v2.serial === 99 && v2.issueDay === day, "the browser rejects a key from apex_keygen");

  check(!(await ApexLicence.verify(k1, devPublic)), "a key checks against another public key");
  const tampered = k1.slice(0, -3) + (k1.at(-3) === "Z" ? "Y" : "Z") + k1.slice(-2);
  check(!(await ApexLicence.verify(tampered, kp.publicHex)), "a changed key passes");
  check(!!(await ApexLicence.verify(k1.toLowerCase().replace(/-/g, " "), kp.publicHex)), "lower case without dashes is refused");
  let threw = false;
  try { await ApexLicence.importSecret(kp.secretHex.slice(0, 64) + devPublic); } catch { threw = true; }
  check(threw, "a secret key with halves that do not belong together is accepted");

  const k3 = await ApexLicence.sign(fresh, { products: 1, issueDay: day, serial: 1, owner: "Ö".repeat(40) });
  const v3 = await ApexLicence.verify(k3, kp.publicHex);
  check(v3 && v3.owner === "Ö".repeat(30), "a long owner is not cut at a character boundary");
  check(run("check", k3, "--public", kp.publicHex).startsWith("valid"), "apex_keygen rejects a key with a cut owner");

  const example = html.match(/EXAMPLE_KEY = "([^"]+)"/)[1];
  check((await ApexLicence.verify(example, devPublic))?.owner === "Cross Check", "the page's example key does not check");
} finally {
  rmSync(tmp, { recursive: true, force: true });
}
console.log(failures ? `FAIL (${failures})` : "PASS");
process.exit(failures ? 1 : 0);
