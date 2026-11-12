"""Require CI from the designated workflow, repository, commit and run attempt."""
import json
import os
import re
import subprocess
from urllib.parse import urlencode

WORKFLOW = ".github/workflows/build.yml"


def select_run(workflow, runs, repository, sha):
    if workflow["path"] != WORKFLOW or workflow["state"] != "active":
        raise ValueError("The required build workflow is not active")
    candidates = [r for r in runs if r["workflow_id"] == workflow["id"]
                  and r["path"] == WORKFLOW and r["head_sha"] == sha
                  and r["head_branch"] == "main" and r["event"] == "push"
                  and r["repository"]["full_name"] == repository
                  and r["head_repository"]["full_name"] == repository]
    if not candidates:
        raise ValueError("No main push run of the required build workflow for this commit")
    # Do not fall back to an older success when the latest run/attempt failed or
    # is still pending. API responses are not relied on for sort order.
    latest = max(candidates, key=lambda r: (r["run_number"], r["id"], r["run_attempt"]))
    if latest["status"] != "completed" or latest["conclusion"] != "success":
        raise ValueError("The latest required CI run is not successful; retry after CI passes")
    return latest


def validate_jobs(run, jobs, sha):
    for name in ("linux", "windows"):
        matches = [j for j in jobs if j["name"] == name]
        if len(matches) != 1:
            raise ValueError(f"Expected exactly one {name} job in the selected CI attempt")
        job = matches[0]
        if (job["run_id"] != run["id"] or job["run_attempt"] != run["run_attempt"]
                or job["head_sha"] != sha or job["status"] != "completed"
                or job["conclusion"] != "success"):
            raise ValueError(f"Required {name} job is not successful in the selected CI attempt")


def validate_ci(repository, sha, api):
    if not re.fullmatch(r"[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+", repository) or not re.fullmatch(r"[0-9a-fA-F]{40}", sha):
        raise ValueError("Invalid repository or commit")
    base = f"repos/{repository}/actions"
    workflow = api(f"{base}/workflows/build.yml")
    query = urlencode({"head_sha": sha, "branch": "main", "event": "push", "per_page": 100})
    listing = api(f"{base}/workflows/{workflow['id']}/runs?{query}")
    if listing["total_count"] != len(listing["workflow_runs"]):
        raise ValueError("Incomplete CI runs response; cannot select the latest run safely")
    run = select_run(workflow, listing["workflow_runs"], repository, sha)
    # Use the attempt-specific endpoint so an earlier successful attempt cannot
    # supply a job omitted, skipped or failed in the latest attempt.
    jobs = []
    page = 1
    while True:
        data = api(f"{base}/runs/{run['id']}/attempts/{run['run_attempt']}/jobs?per_page=100&page={page}")
        jobs.extend(data["jobs"])
        if len(jobs) >= data["total_count"]:
            break
        if not data["jobs"]:
            raise ValueError("Incomplete CI jobs response")
        page += 1
    validate_jobs(run, jobs, sha)
    current = select_run(workflow, [api(f"{base}/runs/{run['id']}")], repository, sha)
    if current["run_attempt"] != run["run_attempt"]:
        raise ValueError("CI was rerun during validation; retry after its current attempt passes")
    return run


def github_api(endpoint):
    result = subprocess.run(["gh", "api", endpoint], check=True, capture_output=True, text=True)
    return json.loads(result.stdout)


if __name__ == "__main__":
    run = validate_ci(os.environ["GITHUB_REPOSITORY"], os.environ["GITHUB_SHA"], github_api)
    print(f"Verified build workflow run {run['id']}, attempt {run['run_attempt']}")
