"""Descriptive statistics and regression used across the book."""

from ofod.stats.descriptive import Regression
from ofod.stats.descriptive import correlation
from ofod.stats.descriptive import covariance
from ofod.stats.descriptive import linear_regression
from ofod.stats.descriptive import mean
from ofod.stats.descriptive import std_dev
from ofod.stats.descriptive import variance

__all__ = [
    "Regression",
    "correlation",
    "covariance",
    "linear_regression",
    "mean",
    "std_dev",
    "variance",
]
