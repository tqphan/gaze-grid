import copy
import importlib.util
from pathlib import Path
import unittest

spec = importlib.util.spec_from_file_location("release_ci", Path(__file__).resolve().parents[1] / "platform/windows/Validate-ReleaseCi.py")
ci = importlib.util.module_from_spec(spec)
spec.loader.exec_module(ci)


class ReleaseCiTests(unittest.TestCase):
    def setUp(self):
        self.repo = "owner/project"
        self.sha = "a" * 40
        self.workflow = {"id": 123, "path": ci.WORKFLOW, "state": "active"}
        self.run = {"id": 1001, "workflow_id": 123, "path": ci.WORKFLOW,
                    "head_sha": self.sha, "head_branch": "main", "event": "push",
                    "repository": {"full_name": self.repo}, "head_repository": {"full_name": self.repo},
                    "run_number": 10, "run_attempt": 2, "status": "completed", "conclusion": "success"}
        self.jobs = [{"name": name, "run_id": 1001, "run_attempt": 2, "head_sha": self.sha,
                      "status": "completed", "conclusion": "success"} for name in ("linux", "windows")]

    def select(self, runs=None):
        return ci.select_run(self.workflow, [self.run] if runs is None else runs, self.repo, self.sha)

    def test_successful_designated_workflow_and_attempt(self):
        ci.validate_jobs(self.select(), self.jobs, self.sha)

    def test_same_name_jobs_in_another_workflow_do_not_count(self):
        self.run["workflow_id"] = 999
        with self.assertRaises(ValueError):
            self.select()

    def test_wrong_workflow_path_rejected(self):
        self.run["path"] = ".github/workflows/counterfeit.yml"
        with self.assertRaises(ValueError):
            self.select()

    def test_pr_dispatch_branch_fork_and_wrong_commit_rejected(self):
        for changes in ({"event": "pull_request"}, {"event": "workflow_dispatch"},
                        {"head_branch": "feature"}, {"head_sha": "b" * 40},
                        {"repository": {"full_name": "attacker/project"}},
                        {"head_repository": {"full_name": "attacker/project"}}):
            with self.subTest(changes=changes):
                candidate = dict(self.run, **changes)
                with self.assertRaises(ValueError):
                    self.select([candidate])

    def test_inactive_workflow_rejected(self):
        self.workflow["state"] = "disabled_manually"
        with self.assertRaises(ValueError):
            self.select()

    def test_latest_failed_or_pending_run_blocks_older_success(self):
        for state, conclusion in (("completed", "failure"), ("in_progress", None)):
            with self.subTest(state=state):
                newer = dict(self.run, id=1002, run_number=11, status=state, conclusion=conclusion)
                with self.assertRaises(ValueError):
                    self.select([newer, self.run])

    def test_current_attempt_cannot_borrow_old_attempt_jobs(self):
        self.jobs[1]["run_attempt"] = 1
        with self.assertRaises(ValueError):
            ci.validate_jobs(self.run, self.jobs, self.sha)

    def test_missing_duplicate_skipped_failed_or_foreign_job_rejected(self):
        cases = [self.jobs[:1], self.jobs + [self.jobs[0]]]
        for changes in ({"conclusion": "skipped"}, {"conclusion": "failure"},
                        {"status": "in_progress"}, {"run_id": 999}, {"head_sha": "b" * 40}):
            candidate = copy.deepcopy(self.jobs)
            candidate[1].update(changes)
            cases.append(candidate)
        for jobs in cases:
            with self.subTest(jobs=jobs), self.assertRaises(ValueError):
                ci.validate_jobs(self.run, jobs, self.sha)

    def test_api_queries_workflow_identity_and_exact_attempt_with_pagination(self):
        calls = []

        def api(path):
            calls.append(path)
            if path.endswith("/workflows/build.yml"):
                return self.workflow
            if "/workflows/123/runs?" in path:
                self.assertIn("head_sha=" + self.sha, path)
                self.assertIn("branch=main", path)
                self.assertIn("event=push", path)
                return {"total_count": 1, "workflow_runs": [self.run]}
            if path.endswith("/runs/1001"):
                return self.run
            self.assertIn("/runs/1001/attempts/2/jobs?", path)
            page = 0 if path.endswith("page=1") else 1
            return {"total_count": 2, "jobs": [self.jobs[page]]}

        self.assertEqual(ci.validate_ci(self.repo, self.sha, api), self.run)
        self.assertEqual(len(calls), 5)

    def test_incomplete_jobs_response_rejected(self):
        def api(path):
            if path.endswith("/workflows/build.yml"):
                return self.workflow
            if "/workflows/123/runs?" in path:
                return {"total_count": 1, "workflow_runs": [self.run]}
            return {"total_count": 2, "jobs": []}

        with self.assertRaises(ValueError):
            ci.validate_ci(self.repo, self.sha, api)

    def test_truncated_runs_response_rejected(self):
        def api(path):
            if path.endswith("/workflows/build.yml"):
                return self.workflow
            return {"total_count": 2, "workflow_runs": [self.run]}

        with self.assertRaisesRegex(ValueError, "Incomplete CI runs"):
            ci.validate_ci(self.repo, self.sha, api)

    def test_attempt_changed_during_validation_rejected(self):
        def api(path):
            if path.endswith("/workflows/build.yml"):
                return self.workflow
            if "/workflows/123/runs?" in path:
                return {"total_count": 1, "workflow_runs": [self.run]}
            if path.endswith("/runs/1001"):
                return dict(self.run, run_attempt=3)
            return {"total_count": 2, "jobs": self.jobs}

        with self.assertRaisesRegex(ValueError, "rerun during validation"):
            ci.validate_ci(self.repo, self.sha, api)


if __name__ == "__main__":
    unittest.main()
