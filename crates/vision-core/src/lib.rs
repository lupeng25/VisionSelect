pub mod calculator;
pub mod catalog;
pub mod catalog_corrections;
pub mod catalog_filter;
pub mod engine;
pub mod model;
pub mod store;
pub mod table_import;

pub type Result<T> = std::result::Result<T, String>;
