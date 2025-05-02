import pandas as pd
import numpy as np

# File 1 - 5 rows
df1 = pd.DataFrame({
    'string_col': ['apple', 'banana', 'cherry', 'date', 'elderberry'],
    'int32_col': pd.Series([1, 2, 3, 4, 5], dtype='int32'),
    'int64_col': pd.Series([10, 20, 30, 40, 50], dtype='int64'),
    'float_col': pd.Series([1.1, 2.2, 3.3, 4.4, 5.5], dtype='float32'),
    'double_col': pd.Series([10.01, 20.02, 30.03, 40.04, 50.05], dtype='float64'),
    'bool_col': pd.Series([True, False, True, False, True], dtype='bool')
})

# File 2 - 3 rows
df2 = pd.DataFrame({
    'string_col': ['fig', 'grape', 'honeydew'],
    'int32_col': pd.Series([6, 7, 8], dtype='int32'),
    'int64_col': pd.Series([60, 70, 80], dtype='int64'),
    'float_col': pd.Series([6.6, 7.7, 8.8], dtype='float32'),
    'double_col': pd.Series([60.06, 70.07, 80.08], dtype='float64'),
    'bool_col': pd.Series([False, True, False], dtype='bool')
})

# File 3 - 3  rows
df3 = pd.DataFrame({
    'string_col': ['kiwi', 'lemon'],
    'int32_col': pd.Series([9, 10], dtype='int32'),
    'int64_col': pd.Series([90, 100], dtype='int64'),
    'float_col': pd.Series([9.9, 10.1], dtype='float32'),
    'double_col': pd.Series([90.09, 100.10], dtype='float64'),
    'bool_col': pd.Series([True, False], dtype='bool')
})

# Save as Parquet files
df1.to_parquet("file_1.parquet", engine="pyarrow")
df2.to_parquet("file_2.parquet", engine="pyarrow")
df3.to_parquet("file_3.parquet", engine="pyarrow")

print("Files saved with explicit types: int32, int64, float32, float64, and bool.")

